/*
 * Net/GameServer.cpp of the game: the server (start, close, change of level, kick), its frame (heartbeat to the
 * master, the receive loop, the sends to the clients in turn, the clients that timed out), the hex dumps of the net
 * debug output and the answers to the All-Seeing Eye queries (its SDK is linked into the exe at rva 0x2070..; the
 * functions in front of GameClient by their place). See include/scrap/net.h.
 */
#include <string.h>

#include "scrap/app.h"
#include "scrap/console.h"
#include "scrap/crt.h"
#include "scrap/net.h"
#include "scrap/python.h"

static const char *Net_TeamName(int team) {
	switch (team) {
	case 0:
		return "Red";
	case 1:
		return "Blue";
	case 2:
		return "Green";
	default:
		return "None";
	}
}

/* the player "net<n>" for the ASE player list */
void Net_AsePlayer(int n) {
	char name[0x20], score[0x20], ping[0x20];

	crt_sprintf(name, "net%d", n);
	cEntity *e = World_FindEntity(name);
	if (!e || !e->vftable->IsType(e, TYEN_CAR)) {
		return;
	}
	const char *team = Net_TeamName(NetEnt_Team(e));
	Str_PrintfQuiet(ping, sizeof(ping), "%d", Net_GetPing(name));
	score[0] = 0;
	PyObject *o = PyImport_AddModule("Ffa");
	if (o && (o = PyModule_GetDict(o)) && (o = PyDict_GetItemString(o, "Scorer")) &&
		(o = PyDict_GetItemString(o, name))) {
		Str_PrintfQuiet(score, sizeof(score), "%d", (int)((PyIntObject *)o)->ob_ival);
	}
	Ase_AddPlayer(NetEnt_UsrName(e) ? NetEnt_UsrName(e) : "", team, NetEnt_TypeName(e), score, ping, 0);
}

void Net_AseShutdown(void) {
	if (g_AseInit) {
		Ase_Shutdown();
	}
}

static void Net_AseError(void) {
	Con_Print(0xff, g_AseError, 0);
	Con_Print(0xff, "\n", 0);
}

void Net_AseInit(int port) {
	if (g_AseInit && !Ase_Initialize(port, 1, 0)) {
		Net_AseError();
	}
}

void Net_AseCheck(void) {
	if (g_AseInit && !Ase_Check()) {
		Net_AseError();
	}
}

/* ASEQuery_wantplayers: the own ship of a playing server, then the clients */
void Net_AsePlayers(void) {
	if (!(g_pApp->flags & APPF_DEDICATED)) {
		Net_AsePlayer(0x100);
	}
	for (int i = 0; i < Transport_GetMaxClients(); i++) {
		Net_AsePlayer(i);
	}
}

/* ASEQuery_wantrules */
void Net_AseRules(void) {
	char s[12];

	crt_ltoa(g_Fraglimit, s, 10);
	Ase_AddRule("fraglimit", s);
	Ase_AddRule("dedicated", (g_pApp->flags & APPF_DEDICATED) ? "1" : "0");
	Ase_AddRule("ForceServerVehicle", g_ForceServerVehicle ? "1" : "0");
}

/* ASEQuery_wantstatus */
void Net_AseStatus(void) {
	char ver[12];

	Str_PrintfQuiet(ver, 10, "%d.%d", 1, 1);
	const char *type = g_ServerType, *map = g_pApp->levelPath + 7, *name = g_ServerName;
	int self = !((g_pApp->flags >> 2) & 1);
	int max = Transport_GetMaxClients() + self;
	int num = Net_GetSendRate() + self;
	Ase_ServerStatus(name, type, map, ver, g_UseServerPassword != 0, num, max);
}

/* the script callbacks of the server gvars back to none (unless scripts may not write them) */
void Net_ServerResetFuncs(void) {
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

byte Net_CloseServer(const char *msg) {
	byte on = g_NetServerPort != 0;

	if (!on) {
		return on;
	}
	Net_AseShutdown();
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
cUserNetManager *Net_NewClientManager(cNetClient *client, int slot) {
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
	MsgMan_SetUserFactory((netuserfactory_t)Game_Ptr(0x18afd0));
	Net_Init();
	if (g_pApp->vftable->IsDedicatedWin(g_pApp)) {
		Transport_SetMaster(g_MasterServerAddress, (uint16_t)g_MasterServerPort);
	}
	g_NetHeartbeat = 0.0f;
	Net_AseInit(g_NetServerPort);
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

byte Net_ServerFrame(void) {
	char s[0x80];
	int r;

	CHECK(g_NetServerPort != 0);
	if (Transport_GetMaxClients() > Net_GetSendRate() && g_NetBrowserCode && *g_NetBrowserCode) {
		double next = (double)(g_HeartbeatTime + g_NetHeartbeat);
		if (g_pApp->vftable->GetTime(g_pApp) > next) {
			g_NetHeartbeat = (float)g_pApp->vftable->GetTime(g_pApp);
			Str_PrintfQuiet(s, sizeof(s), "HB=%d,%d:%d|%s", NET_GAME_ID, NET_GAME_VERSION, g_NetServerPort,
				g_NetBrowserCode);
			Net_SendMasterString(s);
		}
	}
	Net_AseCheck();
	if (g_pApp->flags & APPF_DEDICATED) {
		Transport_Wait(1000);
	}
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
			u->vftable->Send(u, Net_TimeMs());
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

void Net_HexDump(const void *data, int size) {
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
