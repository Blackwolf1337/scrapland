/*
 * The client side of the game network (Net/GameClient.cpp by its place before GameServer.cpp; no CHECK in it):
 * the connection to a server, the frame of the client and its script callbacks. See include/scrap/net.h.
 */
#include <string.h>

#include "scrap/app.h"
#include "scrap/console.h"
#include "scrap/net.h"

byte Net_CloseClient(void) {
	if (!g_NetClientServer[0]) {
		return 0;
	}
	Net_Done();
	cNetClient_Goodbye(g_NetClient);
	/* the goodbye goes out until the server answers or g_NetCloseTime passes */
	while (cNetClient_Flush(g_NetClient, Net_TimeMs(), 0)) {
	}
	cNetClient *c = g_NetClient;
	if (c) {
		cNetClient_dtor(c);
		operator_delete_sized(c, sizeof(*c));
	}
	g_NetClient = NULL;
	Mem_Free(g_NetClientData);
	Mem_Free(g_NetClientCtl);
	MsgMan_SetUserFactory(NULL);
	g_NetClientServer[0] = 0;
	g_NetClientPort = 0;
	Transport_CloseClient();
	return 1;
}

/* the callbacks of the client gvars back to none (unless scripts may not write them) */
void Net_ClientResetFuncs(void) {
	if (!(g_ClientAddResFlags & 1)) {
		cPy_Function_SetFunc(g_ClientAddRes, "");
	}
	if (!(g_ClientDelResFlags & 1)) {
		cPy_Function_SetFunc(g_ClientDelRes, "");
	}
	if (!(g_ClientCreateFlags & 1)) {
		cPy_Function_SetFunc(g_ClientCreate, "");
	}
	if (!(g_ClientSetPlayerFlags & 1)) {
		cPy_Function_SetFunc(g_ClientSetPlayer, "");
	}
	if (!(g_OnUsrStringFlags & 1)) {
		cPy_Function_SetFunc(g_OnUsrString, "");
	}
	if (!(g_OnChatStringFlags & 1)) {
		cPy_Function_SetFunc(g_OnChatString, "");
	}
}

static void Net_SetStatus(const char *s) {
	if (cString_CmpStr(g_ClientConnectionStatus, s)) {
		cString_Set(g_ClientConnectionStatus, s);
	}
}

byte Net_ClientFrame(void) {
	int slot;
	uint32_t t = Net_TimeMs();

	g_NetClientDataSize = 24000;
	g_NetClientCtlSize = 24000;
	byte got = cNetClient_ClientRecv(g_NetClient, t, &slot, g_NetClientData, &g_NetClientDataSize, g_NetClientCtl,
		&g_NetClientCtlSize) != 0;
	if (cString_CmpStr(g_ClientConnectionStatus, "server_is_full")) {
		switch (g_NetClient->state) {
		case NETCLIENT_CONNECTED:
			Net_SetStatus("connected");
			break;
		case NETCLIENT_WAITING:
			Net_SetStatus("server_is_not_responding");
			break;
		case NETCLIENT_TIMEOUT:
			if (Net_CloseClient()) {
				CWinApp_LoadLevel(g_pApp, "Levels/Menu");
			}
			return 0;
		}
	}
	if (!cNetClient_TimeToSend(g_NetClient, t)) {
		cUserNetManager *u = g_NetClient->user;
		if (u) {
			u->vftable->Send(u, Net_TimeMs());
		}
	}
	return got;
}

void Net_ClientReset(void) {
	Net_ClientResetFuncs();
	Net_Init();
}

byte Net_CloseClientMsg(const char *msg) {
	if (Net_CloseClient()) {
		CWinApp_LoadLevel(g_pApp, msg);
		return 1;
	}
	return 0;
}

/* g_NetUserFactory of a client */
cUserNetManager *Net_NewServerManager(cNetClient *client, int slot) {
	cServerNetManager *m = operator_new(sizeof(cServerNetManager));

	if (!m) {
		return NULL;
	}
	return &cServerNetManager_ctor(m, client, slot)->base;
}

const char *Net_GetClientName(void) {
	return g_NetClientServer;
}

uint16_t Net_GetClientPort(void) {
	return g_NetClientPort;
}

cNetClient *Net_GetClient(void) {
	return g_NetClient;
}

byte Net_InitClient(const char *address, int port) {
	if (g_NetClientServer[0]) {
		/* one argument more than the format, as in the original */
		const char *fmt = "Client already connected at '%s':%i\n";
		Con_Printf(fmt, address, (uint16_t)port, g_NetClientPort);
		return 0;
	}
	if (Net_IsServer()) {
		Con_Printf("Impossible to init client mode, Server running !\n");
		return 0;
	}
	Str_Copy(g_NetClientServer, address, 0x100);
	uint16_t p = (uint16_t)port ? (uint16_t)port : NET_DEFAULT_PORT;
	g_NetClientPort = p;
	if (!Transport_InitClient(address, p)) {
		g_NetClientServer[0] = 0;
		g_NetClientPort = 0;
		return 0;
	}
	int pps = g_ClientPPS;
	if (pps > 20) {
		pps = 20;
	} else if (pps < 1) {
		pps = 1;
	}
	g_ClientPPS = pps;
	MsgMan_SetSendDelay((uint32_t)(1000 / pps));
	MsgMan_SetUserFactory((netuserfactory_t)GAME_FUNC(Net_NewServerManager));
	g_NetClientData = Mem_Alloc(24000);
	g_NetClientCtl = Mem_Alloc(24000);
	cNetClient *c = operator_new(sizeof(cNetClient));
	g_NetClient = c ? cNetClient_ctor(c, -1) : NULL;
	Net_ClientResetFuncs();
	Net_Init();
	SaveVars_Delete("");
	Net_SetStatus("connecting");
	Transport_SetMaster(g_MasterServerAddress, (uint16_t)g_MasterServerPort);
	return 1;
}

byte Net_IsClient(void) {
	return g_NetClientServer[0] != 0;
}

byte Net_ClientReady(void) {
	if (g_NetClientServer[0] && g_NetClient) {
		byte msg = NETMSG_USR_DATA;
		cNetClient_Push(g_NetClient, &msg, 1);
		return 1;
	}
	return 0;
}
