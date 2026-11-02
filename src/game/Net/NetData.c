/*
 * Net/NetData.cpp of the game: the resource names of the network game (the models the server creates on the
 * clients by number), the names of the network entities ("net<n>"), the player descriptors (the ship config of a
 * player), the chat and user strings and the browser. See include/scrap/net.h.
 */
#include <string.h>

#include "scrap/actions.h"
#include "scrap/app.h"
#include "scrap/chartype.h"
#include "scrap/console.h"
#include "scrap/crt.h"
#include "scrap/net.h"
#include "scrap/vehicleentity.h"
#include "scrap/vehiclemotors.h"
#include "scrap/weapons.h"
#include "scrap/staticinit.h"

/* "net<n>" with a name of more than 3 characters */
static inline int Net_IsNetName(const char *name) {
	return strlen(name) > 3 && name[0] == 'n' && name[1] == 'e' && name[2] == 't';
}

static char *Net_StrDup(const char *s) {
	char *p = Mem_Alloc((uint32_t)strlen(s) + 1);
	memcpy(p, s, strlen(s) + 1);
	return p;
}

char *g_NetResources[256];

int Net_AddResource(const char *name) {
	if (!g_pApp->flags) {
		return 0;
	}
	if ((int)strlen(name) + 1 > 0x20) {
		Con_Bluef("ERROR: '%s',\n           is too long to be used as resource name\n", name);
		CHECK(0);
	}
	int i;
	for (i = 0; i < NET_RESOURCES; i++) {
		if (!g_NetResources[i]) {
			g_NetResources[i] = Net_StrDup(name);
			if (g_pApp->flags & APPF_SERVER) {
				byte msg = NETMSG_RESOURCES;
				void *buf = cStaticBuffer_Get(g_NetStaticBuffer, 0x80);
				cFileMem_SetBuffer((&g_NetMsg), buf, 0x60, 0, 0);
				cFileMem_Open(&g_NetMsg.base, "", 6);
				g_NetMsg.base.vftable->Write(&g_NetMsg.base, &msg, 1);
				NetData_WriteResources(&g_NetMsg.base, (uint16_t)i, 1);
				NetServer_SendAll(buf, (uint16_t)cFileMem_Size(&g_NetMsg.base));
				cFileMem_Close(&g_NetMsg.base);
				cStaticBuffer_Unlock(g_NetStaticBuffer);
				Con_Debugf("Send resource (%d)'%s' to all.\n", i, name);
			}
			return i;
		}
		if (!crt_stricmp(g_NetResources[i], name)) {
			return i;
		}
	}
	Con_Warnf("WARNING resource (%d)'%s' cannot be created.\n", i, name);
	return -1;
}

cGVar g_GVarServerType;

void Net_Done(void) {
	for (int i = 0; i < NET_RESOURCES; i++) {
		if (g_NetResources[i]) {
			Mem_Free(g_NetResources[i]);
			g_NetResources[i] = NULL;
		}
	}
	cGVar_SetFlag((&g_GVarServerType), 1, 0);
}

int g_NetBotCounter;

void Net_GetBotName(char *buf) {
	do {
		if (g_NetBotCounter > NET_PLAYER_MAX_ID) {
			g_NetBotCounter = 0x100;
		}
		crt_sprintf(buf, "net%d", g_NetBotCounter);
		g_NetBotCounter++;
	} while (World_FindEntity(buf));
}

int Net_GetPlayerIndex(const char *name) {
	if (Net_IsNetName(name)) {
		return crt_atoi(name + 3);
	}
	return -1;
}

int g_NetObjCounter;

void Net_GetObjName(char *buf) {
	do {
		if (g_NetObjCounter > 32000) {
			g_NetObjCounter = NET_PLAYER_MAX_ID + 1;
		}
		crt_sprintf(buf, "net%d", g_NetObjCounter);
		g_NetObjCounter++;
	} while (World_FindEntity(buf));
}

const char *Net_GetResourceName(int i) {
	if (!g_pApp->flags) {
		return NET_OFFLINE;
	}
	return g_NetResources[i];
}

int Net_GetPing(const char *player) {
	if (g_pApp->flags & APPF_SERVER) {
		cNetClient *c = NetServer_GetClient(Net_GetPlayerIndex(player));
		return c ? c->ping : 0;
	}
	return Net_GetClientPing((uint32_t)Net_GetPlayerIndex(player));
}

byte g_NetResInit;

void Net_Init(void) {
	for (int i = 0; i < NET_RESOURCES; i++) {
		if (g_NetResInit && g_NetResources[i]) {
			Mem_Free(g_NetResources[i]);
		}
		g_NetResources[i] = NULL;
	}
	g_NetResInit = 1;
	g_NetObjCounter = NET_PLAYER_MAX_ID + 1;
	g_NetBotCounter = 0x100;
	cGVar_SetFlag((&g_GVarServerType), 1, Net_IsServer() || Net_IsClient());
	g_MasterCommandCallback = (netmastercmd_t)NetData_OnMasterCommand;
}

void NetData_ReadResources(cAbstractFile *f) {
	uint16_t first = 0;
	byte count = 0;
	char name[0x20];

	f->vftable->Read(f, &first, 2);
	f->vftable->Read(f, &count, 1);
	for (int v = first; v < count + first; v++) {
		if (g_NetResources[v]) {
			cPy_Function_AddParamI(g_ClientDelRes, v);
			cPy_Function_AddParamS(g_ClientDelRes, g_NetResources[v]);
			cPy_Function_Execute(g_ClientDelRes, 0);
			Mem_Free(g_NetResources[v]);
			g_NetResources[v] = NULL;
		}
		cAbstractFile_ReadString(f, name, sizeof(name));
		if (name[0]) {
			g_NetResources[v] = Net_StrDup(name);
			cPy_Function_AddParamI(g_ClientAddRes, v);
			cPy_Function_AddParamS(g_ClientAddRes, g_NetResources[v]);
			cPy_Function_Execute(g_ClientAddRes, 0);
		}
	}
}

void NetData_ReadUsrDescriptors(cAbstractFile *f) {
	uint16_t count = 0, id = 0;
	char name[0x80];

	f->vftable->Read(f, &count, 2);
	for (int k = 0; k < count; k++) {
		f->vftable->Read(f, &id, 2);
		Str_PrintfQuiet(name, sizeof(name), "net%d", id);
		cEntity *e = World_FindEntity(name);
		if (!e || !e->vftable->IsType(e, TYEN_CAR)) {
			Con_Bluef("Quite Strange: Entity %s was created in Netdata.cpp:ReadUsrDescriptor()\n", name);
			continue;
		}
		NetData_ReadUsrDescriptor(f, e);
		cPy_Function_AddParamS(g_ClientSetConfig, e->name);
		cPy_Function_AddParamS(g_ClientSetConfig, Veh(e)->typeName);
		cPy_Function_AddParamI(g_ClientSetConfig, Veh(e)->maxLife);
		cPy_Function_AddParamS(g_ClientSetConfig, WLife(e)->usrName ? WLife(e)->usrName : "");
		cPy_Function_AddParamS(g_ClientSetConfig, Veh_Vtbl(Veh(e))->GetPilot(&(Veh(e))->wl.obj.base, 0));
		for (uint32_t m = 0; m < 4; m++) {
			cPy_Function_AddParamS(g_ClientSetConfig, cVehicleMotors_GetMotorName(NetEnt_Motors(e), m, NULL));
		}
		cPy_Function_AddParamS(g_ClientSetConfig, cWeaponManager_GetWeapList(NetEnt_Weapons(e)));
		cPy_Function_AddParamI(g_ClientSetConfig, WLife(e)->arrow.tgType);
		cPy_Function_Execute(g_ClientSetConfig, 0);
	}
}

void NetData_ReadUsrDescriptor(cAbstractFile *f, cEntity *e) {
	char s[0x80];
	uint16_t maxLife = 0;
	int team;

	cAbstractFile_ReadString8(f, s, sizeof(s));
	cString_Set(&WLife(e)->usrName, s);
	cAbstractFile_ReadString8(f, s, sizeof(s));
	if (cActClassList_Get(g_pActClassList, s)) {
		WithLife_Vtbl(e)->SetActCtrl(e, s);
		/* every action of the class */
		cActCtrl *ctrl = WithLife_Vtbl(e)->GetActCtrl(e);
		for (cAction *a = ctrl->cls->first; a; a = a->next) {
			cActCtrl_PlayAction(WithLife_Vtbl(e)->GetActCtrl(e), a, 0, 1, 1.0f);
		}
	} else {
		e->vftable->SetModel(e, s, 1.0f, 1.0f, 1.0f, 1);
	}
	f->vftable->Read(f, &maxLife, 2);
	Veh(e)->maxLife = maxLife;
	for (uint32_t m = 0; m < 4; m++) {
		cAbstractFile_ReadString8(f, s, sizeof(s));
		cVehicleMotors_SetMotor(NetEnt_Motors(e), m, s, 1.0f, 1.0f, 1.0f);
	}
	cAbstractFile_ReadString8(f, s, sizeof(s));
	Veh_Vtbl(Veh(e))->SetPilot(&(Veh(e))->wl.obj.base, 0, s, 1.0f, 1.0f, 1.0f);
	cAbstractFile_ReadString8(f, s, sizeof(s));
	cWeaponManager_SetWeapList(NetEnt_Weapons(e), s);
	f->vftable->Read(f, &team, 4);
	WLife(e)->arrow.tgType = team;
}

void NetData_WriteResources(cAbstractFile *f, uint16_t first, byte count) {
	uint32_t v = first;

	f->vftable->Write(f, &v, 2);
	f->vftable->Write(f, &count, 1);
	for (int i = first; i < count + first; i++) {
		cAbstractFile_WriteString(f, g_NetResources[i] ? g_NetResources[i] : "");
	}
}

byte Net_SendChatString(const char *text, int arg) {
	return Net_SendString(text, arg, 1);
}

byte Net_SendCtrlString(const char *text) {
	if (Transport_GetMode() != NETMODE_CLIENT || !g_ClientRemotePassword || !*g_ClientRemotePassword) {
		return 0;
	}
	byte msg = NETMSG_CONSOLE;
	void *buf = cStaticBuffer_Get(g_NetStaticBuffer, 24000);
	cFileMem_SetBuffer((&g_NetMsg), buf, 24000, 0, 0);
	cFileMem_Open(&g_NetMsg.base, "", 6);
	g_NetMsg.base.vftable->Write(&g_NetMsg.base, &msg, 1);
	cAbstractFile_WriteString8(&g_NetMsg.base, g_ClientRemotePassword);
	cAbstractFile_WriteString8(&g_NetMsg.base, text);
	cNetClient *c = Net_GetClient();
	if (c) {
		cNetClient_Push(c, buf, (uint16_t)cFileMem_Size(&g_NetMsg.base));
	}
	cFileMem_Close(&g_NetMsg.base);
	cStaticBuffer_Unlock(g_NetStaticBuffer);
	return 1;
}

byte Net_SendString(const char *text, int slot, byte chat) {
	char s[0x80];
	byte ok = 1, msg;
	cNetClient *c;

	void *buf = cStaticBuffer_Get(g_NetStaticBuffer, 24000);
	cFileMem_SetBuffer((&g_NetMsg), buf, 24000, 0, 0);
	cFileMem_Open(&g_NetMsg.base, "", 6);
	Str_Copy(s, text, sizeof(s));
	switch (Transport_GetMode()) {
	case NETMODE_SERVER:
		msg = chat ? NETMSG_CHAT : NETMSG_USR_STRING;
		g_NetMsg.base.vftable->Write(&g_NetMsg.base, &msg, 1);
		cAbstractFile_WriteString8(&g_NetMsg.base, s);
		if (slot == -1) {
			NetServer_SendAll(buf, (uint16_t)cFileMem_Size(&g_NetMsg.base));
			break;
		}
		c = NetServer_GetClient(slot);
		if (!c) {
			ok = 0;
			break;
		}
		cNetClient_Push(c, buf, (uint16_t)cFileMem_Size(&g_NetMsg.base));
		break;
	case NETMODE_CLIENT:
		msg = chat ? NETMSG_CHAT : NETMSG_USR_STRING;
		g_NetMsg.base.vftable->Write(&g_NetMsg.base, &msg, 1);
		cAbstractFile_WriteString8(&g_NetMsg.base, s);
		/* a client without connection still answers 1 */
		c = Net_GetClient();
		if (c) {
			cNetClient_Push(c, buf, (uint16_t)cFileMem_Size(&g_NetMsg.base));
		}
		break;
	default:
		ok = 0;
		break;
	}
	cFileMem_Close(&g_NetMsg.base);
	cStaticBuffer_Unlock(g_NetStaticBuffer);
	return ok;
}

byte Net_SendUsrString(const char *text, int arg) {
	return Net_SendString(text, arg, 0);
}

void NetData_OnMasterCommand(const char *cmd) {
	cPy_Function_AddParamS(g_MasterCommandFunc, cmd);
	cPy_Function_Execute(g_MasterCommandFunc, 0);
}

void NetData_WriteUsr(cAbstractFile *f, uint16_t slot) {
	char name[0x80];
	uint32_t v = 1;

	f->vftable->Write(f, &v, 2);
	v = slot;
	f->vftable->Write(f, &v, 2);
	Str_PrintfQuiet(name, sizeof(name), "net%d", slot);
	cEntity *e = World_FindEntity(name);
	CHECK((e != NULL) && e->vftable->IsType(e, TYEN_CAR));
	NetData_WriteUsrDescriptor(f, e);
}

void NetData_WriteUsrDescriptor(cAbstractFile *f, cEntity *e) {
	uint32_t v;

	cAbstractFile_WriteString8(f, WLife(e)->usrName ? WLife(e)->usrName : "");
	cAbstractFile_WriteString8(f, Veh(e)->typeName);
	v = (uint16_t)Veh(e)->maxLife;
	f->vftable->Write(f, &v, 2);
	for (uint32_t m = 0; m < 4; m++) {
		cAbstractFile_WriteString8(f, cVehicleMotors_GetMotorName(NetEnt_Motors(e), m, NULL));
	}
	cAbstractFile_WriteString8(f, Veh_Vtbl(Veh(e))->GetPilot(&(Veh(e))->wl.obj.base, 0));
	cAbstractFile_WriteString8(f, cWeaponManager_GetWeapList(NetEnt_Weapons(e)));
	v = (uint32_t)WLife(e)->arrow.tgType;
	f->vftable->Write(f, &v, 4);
}

cEntity *NetData_WriteUsrDescriptors(cAbstractFile *f, cEntity *e) {
	uint32_t count = 0, v = 0;

	int start = f->vftable->Tell(f);
	f->vftable->Write(f, &v, 2);
	while (e && (int)(uint16_t)count < g_NetMaxPlayersInitSend) {
		if (Net_IsNetName(e->name) && e->vftable->IsType(e, TYEN_CAR)) {
			v = (uint16_t)crt_atoi(e->name + 3);
			f->vftable->Write(f, &v, 2);
			NetData_WriteUsrDescriptor(f, e);
			count++;
		}
		e = cEntity_Next(e);
	}
	int end = f->vftable->Tell(f);
	f->vftable->Seek(f, start);
	v = (uint16_t)count;
	f->vftable->Write(f, &v, 2);
	f->vftable->Seek(f, end);
	return e;
}

byte Net_GetNetIndex(const char *name, int *n) {
	if (Net_IsNetName(name)) {
		*n = crt_atoi(name + 3);
		return 1;
	}
	return 0;
}

byte Net_IsNetEntity(cEntity *e) {
	const char *name = e->name;

	return strlen(name) > 3 && (g_pApp->flags & APPF_CLIENT) && name[0] == 'n' && name[1] == 'e' && name[2] == 't';
}

byte Net_IsPlayerName(const char *name) {
	if (Net_IsNetName(name)) {
		return (uint32_t)crt_atoi(name + 3) < 0x100;
	}
	return 0;
}

int Net_DoneBrowser(void) {
	if (!(g_OnNetBrowseFlags & 1)) {
		cPy_Function_SetFunc(g_OnNetBrowseCallback, "");
	}
	return Transport_CloseBrowser();
}

int g_NetBrowserPort;

int Net_InitBrowser(int a, const char *address) {
	if (!a) {
		a = NET_DEFAULT_PORT;
	}
	g_NetBrowserPort = a;
	if (!Transport_InitBrowser(address, (uint16_t)a)) {
		return 0;
	}
	int r = Transport_PingMaster();
	if (r) {
		cPy_Function_SetFunc(g_OnNetBrowseCallback, "");
	}
	return r;
}

void Net_Update(void) {
	byte buf[0x400];

	if (Transport_GetMode() == NETMODE_BROWSER) {
		int len = sizeof(buf);
		Transport_Recv(buf, &len);
	}
}

void NetData_OnBrowse(const char *ip, int port, const NetServerInfo *info, int ping) {
	if (info->id != NET_GAME_ID || info->version != NET_GAME_VERSION) {
		return;
	}
	Con_Printf("Server %02x '%s':%s (%d/%d) Ver %d.%d at %s:%d\n", info->flags, info->name, info->map,
		info->numPlayers, info->maxPlayers, 1, 1, ip, port);
	cPy_Function_AddParamS(g_OnNetBrowseCallback, ip);
	cPy_Function_AddParamI(g_OnNetBrowseCallback, info->flags);
	cPy_Function_AddParamS(g_OnNetBrowseCallback, info->name);
	cPy_Function_AddParamI(g_OnNetBrowseCallback, info->numPlayers);
	cPy_Function_AddParamI(g_OnNetBrowseCallback, info->maxPlayers);
	cPy_Function_AddParamI(g_OnNetBrowseCallback, info->version);
	cPy_Function_AddParamI(g_OnNetBrowseCallback, port);
	cPy_Function_AddParamS(g_OnNetBrowseCallback, info->type);
	cPy_Function_AddParamI(g_OnNetBrowseCallback, ping);
	cPy_Function_AddParamS(g_OnNetBrowseCallback, info->map);
	cPy_Function_Execute(g_OnNetBrowseCallback, 0);
}

cGVar GVar_NetMaxPlayersInitSend;

cGVarFunction GVar_ClientSetConfig;

cGVarFunction GVar_MasterCommandFunc;

cGVarFunction GVar_ClientAddRes;

cGVar GVar_ClientRemotePassword;

cGVarFunction GVar_OnNetBrowseCallback;

cGVarFunction GVar_ClientDelRes;

/* the dynamic initializers of the globals here, run in the order of src/standalone/crtxc.inc */
INIT_GVAR_INT(Init_GVar_NetMaxPlayersInitSend, &GVar_NetMaxPlayersInitSend, "NetMaxPlayersInitSend", 0, "Maximun number of net players send per packet at the startup.", 4)
FINI_CALL(Fini_GVar_ClientAddRes, &GVar_ClientAddRes.func, cPy_Function_dtor)
INIT_GVAR_FUNCTION(Init_GVar_ClientAddRes, Fini_GVar_ClientAddRes, &GVar_ClientAddRes, "ClientAddRes", 0, "ClientAddRes(Num,Descriptor)", "")
FINI_CALL(Fini_GVar_ClientDelRes, &GVar_ClientDelRes.func, cPy_Function_dtor)
INIT_GVAR_FUNCTION(Init_GVar_ClientDelRes, Fini_GVar_ClientDelRes, &GVar_ClientDelRes, "ClientDelRes", 0, "ClientDelRes(Num,Descriptor)", "")
FINI_CALL(Fini_GVar_ClientSetConfig, &GVar_ClientSetConfig.func, cPy_Function_dtor)
INIT_GVAR_FUNCTION(Init_GVar_ClientSetConfig, Fini_GVar_ClientSetConfig, &GVar_ClientSetConfig, "ClientSetConfig", 0, "ClientSetConfig(entityname,ModelName,MaxLife,UsrName,PilotName,Motor0,Motor1,Motor2,Motor3,WeaponBay,tgtype) player ship modifed", "")
FINI_GVAR_STRING(Fini_GVar_ClientRemotePassword, &GVar_ClientRemotePassword)
INIT_GVAR_STRING(Init_GVar_ClientRemotePassword, Fini_GVar_ClientRemotePassword, &GVar_ClientRemotePassword, "ClientRemotePassword", 6, "Password for client remote control(client). If empty, no remote control allowed.", "")
FINI_CALL(Fini_GVar_OnNetBrowseCallback, &GVar_OnNetBrowseCallback.func, cPy_Function_dtor)
INIT_GVAR_FUNCTION(Init_GVar_OnNetBrowseCallback, Fini_GVar_OnNetBrowseCallback, &GVar_OnNetBrowseCallback, "OnNetBrowseCallback", 0, "Called every time that server is found in a browsing operation.", "")
FINI_CALL(Fini_GVar_MasterCommandFunc, &GVar_MasterCommandFunc.func, cPy_Function_dtor)
INIT_GVAR_FUNCTION(Init_GVar_MasterCommandFunc, Fini_GVar_MasterCommandFunc, &GVar_MasterCommandFunc, "MasterCommandFunc", 0, "MasterCommandFunc(string) (master server sends data)", "")
FINI_GVAR_STRING(Fini_GVar_ServerType, &g_GVarServerType)
INIT_GVAR_STRING(Init_GVar_ServerType, Fini_GVar_ServerType, &g_GVarServerType, "ServerType", 14, "Type of server game.\n", "DeathMatch")
