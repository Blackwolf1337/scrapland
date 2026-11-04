/*
 * The client table of the server (Net/, between NetData.cpp and transport.cpp, no CHECK of its own): one
 * cNetClient for each transport slot in g_NetClients, the broadcasts, the rate limit, the server info of the
 * browser ping and the receive of a packet with the hello check of new clients. See include/scrap/net.h.
 */
#include <string.h>

#include "scrap/console.h"
#include "scrap/net.h"
#include "scrap/staticinit.h"

#define g_UseServerPassword (GVar_UseServerPassword.value.i)
#define g_ForceServerVehicle (GVar_ForceServerVehicle.value.i)
#define g_ServerPassword (((cString *)&GVar_ServerPassword.value))
#define g_ServerName (GVar_ServerName.value.s)
#define g_ServerDataRateLimit (GVar_ServerDataRateLimit.value.i)
static cGVar GVar_UseServerPassword;
static cGVar GVar_ServerPassword;
static cGVar GVar_ServerName;
static cGVar GVar_ServerDataRateLimit;
static cGVar GVar_ForceServerVehicle;

static void NetServer_Free(cNetClient *c) {
	cNetClient_dtor(c);
	operator_delete(c);
}

/* the server side client table */
static cNetClient **g_NetClients;

void NetServer_SendAll(const void *data, uint16_t size) {
	for (int i = 0; i < Transport_GetMaxClients(); i++) {
		cNetClient *c = g_NetClients[i];
		if (c) {
			cNetClient_Push(c, data, size);
		}
	}
}

void NetServer_SendAllBut(const void *data, uint16_t size, int slot) {
	for (int i = 0; i < Transport_GetMaxClients(); i++) {
		cNetClient *c = g_NetClients[i];
		if (c && c->user->slot != slot) {
			cNetClient_Push(c, data, size);
		}
	}
}

static uint32_t g_NetNextSend;

int NetServer_SendTo(uint32_t time, int slot, int size, const void *data) {
	if (Transport_GetMaxClients() <= slot || !g_NetClients[slot]) {
		return 0;
	}
	if (time < g_NetNextSend) {
		return 0;
	}
	int n = (int)cNetClient_Send(g_NetClients[slot], time, slot, size, data);
	int rate = g_ServerDataRateLimit;
	if (rate <= 0x200) {
		rate = 0x200;
	} else if (rate >= 0xa00000) {
		rate = 0xa00000;
	}
	g_ServerDataRateLimit = rate;
	g_NetNextSend = (uint32_t)(n * 1000) / (uint32_t)rate + time;
	return n;
}

void NetServer_Done(void) {
	for (int i = 0; i < Transport_GetMaxClients(); i++) {
		cNetClient *c = g_NetClients[i];
		if (c) {
			NetServer_Free(c);
		}
	}
	Mem_Free(g_NetClients);
	Transport_Close();
}

void NetServer_GetInfo(NetServerInfo *info) {
	Str_Copy(info->name, g_ServerName, sizeof(info->name));
	Str_Copy(info->type, g_ServerType, sizeof(info->type));
	Str_Copy(info->map, g_pApp->levelPath + 7, sizeof(info->map));
	info->id = NET_GAME_ID;
	info->version = NET_GAME_VERSION;
	info->port = Transport_GetLocalPort();
	info->maxPlayers = (int16_t)Transport_GetMaxClients();
	int16_t num = (int16_t)Net_GetSendRate();
	info->numPlayers = num;
	info->flags = ((g_pApp->flags >> 2) & 1) | (g_UseServerPassword ? 4 : 0) | (g_ForceServerVehicle ? 2 : 0);
	/* a server that plays counts itself */
	if (!(g_pApp->flags & APPF_DEDICATED)) {
		info->maxPlayers++;
		info->numPlayers = num + 1;
	}
}

int NetServer_GetSlotByIndex(int n) {
	int k = 0;

	for (int i = 0; i < Transport_GetMaxClients(); i++) {
		if (g_NetClients[i] && k++ == n) {
			return i;
		}
	}
	return -1;
}

cNetClient *NetServer_GetClient(int slot) {
	/* slot == the maximum passes, as in the original */
	if (Transport_GetMaxClients() >= slot && slot >= 0) {
		return g_NetClients[slot];
	}
	return NULL;
}

int NetServer_NextToSend(uint32_t time) {
	int best = -1000 / g_ClientPPS, found = -1;

	for (int i = 0; i < Transport_GetMaxClients(); i++) {
		cNetClient *c = g_NetClients[i];
		if (c) {
			int d = (int)(c->lastSend - time);
			if (d <= best) {
				best = d;
				found = i;
			}
		}
	}
	return found;
}

int NetServer_Init(int maxClients, uint16_t port, int arg) {
	(void)arg;
	if (!Transport_InitServer(maxClients, port)) {
		return 0;
	}
	g_NetClients = Mem_Alloc(maxClients * sizeof(*g_NetClients));
	memset(g_NetClients, 0, maxClients * sizeof(*g_NetClients));
	g_NetNextSend = 0;
	return 1;
}

void NetServer_Goodbye(void) {
	for (int i = 0; i < Transport_GetMaxClients(); i++) {
		cNetClient *c = g_NetClients[i];
		if (c) {
			cNetClient_Goodbye(c);
		}
	}
}

int NetServer_DropClient(int slot) {
	int r = Transport_RemoveClient(slot);

	if (r) {
		cNetClient *c = g_NetClients[slot];
		if (c) {
			NetServer_Free(c);
		}
		g_NetClients[slot] = NULL;
	}
	return r;
}

int NetServer_Flush(uint32_t time) {
	byte any = 0;

	for (int i = 0; i < Transport_GetMaxClients(); i++) {
		cNetClient *c = g_NetClients[i];
		if (!c) {
			continue;
		}
		if (c->state == NETCLIENT_TIMEOUT) {
			NetServer_Free(c);
			g_NetClients[i] = NULL;
		} else {
			cNetClient_Flush(c, time, i);
			any = 1;
		}
	}
	return any;
}

byte Net_ModifyUsrData(int arg) {
	if (!Net_IsServer()) {
		Con_Warnf("WARNING : ServerModifyUsrData() : Not server mode.\n");
		return 0;
	}
	if (Transport_GetMaxClients() <= arg) {
		/* the slots above the clients: the bots */
		if ((uint32_t)(arg - 0x100) <= 0x100) {
			ManServer_SendUsrData(arg, 1);
			return 1;
		}
		Con_Warnf("WARNING : ServerModifyUsrData(%d) : Out of clients range.\n", arg);
		return 0;
	}
	cNetClient *c = g_NetClients[arg];
	if (!c) {
		Con_Warnf("WARNING : ServerModifyUsrData(%d) : Client does not exists.\n", arg);
		return 0;
	}
	NetMan_SetLocalData(c->user);
	return 1;
}

int NetServer_Receive(uint32_t time, void *data, int *dataSize, void *ctl, int *ctlSize) {
	int len = *ctlSize + (*dataSize + 8);
	byte *buf = cStaticBuffer_Get(g_PyStaticBuffer, len);
	int before = Net_GetSendRate();
	int slot = Transport_Recv(buf, &len);

	if (slot == -1 || len < 3) {
		cStaticBuffer_Unlock(g_PyStaticBuffer);
		return -1;
	}
	if (Net_GetSendRate() > before) {
		/* a new client: its first packet must be a hello of this game */
		uint16_t n, id, ver;
		memcpy(&n, buf + 1, 2);
		memcpy(&id, buf + 5, 2);
		memcpy(&ver, buf + 7, 2);
		if ((buf[0] & 0x7f) != 0x7f || (buf[3] & 0x7f) || buf[4] != NETMSG_DELETE || !n || id != NET_GAME_ID ||
			ver != NET_GAME_VERSION) {
			cStaticBuffer_Unlock(g_PyStaticBuffer);
			Transport_RemoveClient(slot);
			return -2;
		}
		if (g_UseServerPassword) {
			int pn = buf[9];
			if (cString_Length(g_ServerPassword) != pn || (pn && memcmp(*g_ServerPassword, buf + 10, pn))) {
				Transport_SendNeedPassword(slot);
				cStaticBuffer_Unlock(g_PyStaticBuffer);
				Transport_RemoveClient(slot);
				return -2;
			}
		}
		if (!g_NetClients[slot]) {
			cNetClient *c = operator_new(sizeof(cNetClient));
			g_NetClients[slot] = c ? cNetClient_ctor(c, slot) : NULL;
		}
	}
	cNetClient_Receive(g_NetClients[slot], time, buf, len, data, dataSize, ctl, ctlSize);
	cStaticBuffer_Unlock(g_PyStaticBuffer);
	return slot;
}

static cGVar GVar_UseServerPassword;
static cGVar GVar_ForceServerVehicle;
static cGVar GVar_ServerPassword;
static cGVar GVar_ServerName;
static cGVar GVar_ServerDataRateLimit;

/* the dynamic initializers of the globals here, run in the order of src/standalone/crtxc.inc */
INIT_GVAR_INT(Init_GVar_ForceServerVehicle, &GVar_ForceServerVehicle, "ForceServerVehicle", 6, "All the ships will use the server's one\n", 0)
FINI_GVAR_STRING(Fini_GVar_ServerPassword, &GVar_ServerPassword)
INIT_GVAR_STRING(Init_GVar_ServerPassword, Fini_GVar_ServerPassword, &GVar_ServerPassword, "ServerPassword", 6, "The server's password.\n", "dtritus")
INIT_GVAR_INT(Init_GVar_UseServerPassword, &GVar_UseServerPassword, "UseServerPassword", 6, "if the server needs password to join, set to true.", 0)
FINI_GVAR_STRING(Fini_GVar_ServerName, &GVar_ServerName)
INIT_GVAR_STRING(Init_GVar_ServerName, Fini_GVar_ServerName, &GVar_ServerName, "ServerName", 6, "Name of the network server\n", "Unnamed Server")
INIT_GVAR_INT(Init_GVar_ServerDataRateLimit, &GVar_ServerDataRateLimit, "ServerDataRateLimit", 2, "Rate Limit in bytes per second\n", 8192)
