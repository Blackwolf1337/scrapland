/*
 * Net/ManClient.cpp of the game: cServerNetManager, the user manager of a client for its connection to the server.
 * It reads the entity stream and the messages of the server and sends the own ship. See include/scrap/net.h.
 */
#include <string.h>

#include "scrap/app.h"
#include "scrap/console.h"
#include "scrap/crt.h"
#include "scrap/net.h"
#include "scrap/objentity.h"
#include "scrap/staticinit.h"

cServerNetManager *cServerNetManager_ctor(cServerNetManager *self, cNetClient *client, int slot) {
	cUserNetManager_ctor(&self->base, client, slot);
	self->base.vftable = g_ServerNetManagerVftable;
	Con_Print(0, "Try to server\n", 0);
	cServerNetManager_SendHello(&self->base, NETMSG_DELETE);
	self->name[0] = 0;
	return self;
}

void cServerNetManager_dtor(cServerNetManager *self) {
	self->base.vftable = g_ServerNetManagerVftable;
	Con_Print(0, "disconected!\n", 0);
	cUserNetManager_dtor(&self->base);
}

cUserNetManager *cServerNetManager_Delete(cUserNetManager *base, byte flags) {
	cServerNetManager *self = (cServerNetManager *)base;
	cServerNetManager_dtor(self);
	if (flags & 1) {
		operator_delete_sized(self, sizeof(*self));
	}
	return base;
}

byte g_NetClientPings[256];

cFileMem g_NetMsg;

/* the entities of the stream: an id ("net<id>") and a resource byte each, then the state of the entity */
static void ServerNet_ReadEntities(cServerNetManager *self, int size) {
	char name[0x80];
	const char *res = NULL;
	uint16_t plyId, numVals;
	byte idx, ping, hdr[3];
	int start = 0;

	g_NetMsg.base.vftable->Read(&g_NetMsg.base, &plyId, 2);
	g_NetMsg.base.vftable->Read(&g_NetMsg.base, &numVals, 2);
	g_NetMsg.base.vftable->Read(&g_NetMsg.base, &g_NetOrigin.x, 4);
	g_NetMsg.base.vftable->Read(&g_NetMsg.base, &g_NetOrigin.y, 4);
	g_NetMsg.base.vftable->Read(&g_NetMsg.base, &g_NetOrigin.z, 4);
	g_NetMsg.base.vftable->Read(&g_NetMsg.base, &idx, 1);
	g_NetMsg.base.vftable->Read(&g_NetMsg.base, &ping, 1);
	g_NetClientPings[idx] = ping;
	if (g_LogNetData) {
		Con_Verbosef("Size: %04d   PlyId: %03d   NumVals: %03d\n", size, plyId, numVals);
	}
	for (int i = 0; i < numVals; i++) {
		cFileMem_Read(&g_NetMsg.base, hdr, 3);
		uint16_t id = (uint16_t)(hdr[0] | hdr[1] << 8);
		if (g_LogNetData) {
			Con_Verbosef("  (%2d:'net%3d'-%02x) ", i, id, hdr[2]);
		}
		crt_sprintf(name, "net%d", id);
		cEntity *e = World_FindEntity(name);
		byte isNew = e == NULL, created = 0;
		if (!e) {
			if (id <= NET_PLAYER_MAX_ID) {
				e = cWorld_CreateEntity(g_pWorld, name, 0.0f, 0.0f, 0.0f, TYEN_CAR);
				if (i == plyId) {
					Str_Copy(self->name, name, sizeof(self->name));
					cPy_Function_AddParamS(g_ClientSetPlayer, name);
					cPy_Function_Execute(g_ClientSetPlayer, 0);
				}
			} else {
				res = Net_GetResourceName(hdr[2]);
				if (res) {
					cPy_Function_AddParamS(g_ClientCreate, name);
					cPy_Function_AddParamS(g_ClientCreate, res);
					cPy_Function_Execute(g_ClientCreate, 0);
					e = World_FindEntity(name);
					created = 1;
				}
			}
			if (!e) {
				cNetClient_RequestFull(Net_GetClient());
				return;
			}
		}
		if (g_LogNetData) {
			start = cFileMem_Tell(&g_NetMsg.base);
		}
		Obj_Vtbl(e)->NetRead(e, (&g_NetMsg), isNew);
		if (g_LogNetData) {
			int end = cFileMem_Tell(&g_NetMsg.base);
			cFileMem_Seek(&g_NetMsg.base, start);
			for (int p = start; p < end; p++) {
				Con_Verbosef("%02x", g_NetMsg.buffer[p]);
			}
			Con_Print(0x707000, "\n", 0);
			cFileMem_Seek(&g_NetMsg.base, end);
		}
		if (created) {
			cPy_Function_AddParamS(g_ClientAfterCreate, e->name);
			cPy_Function_AddParamS(g_ClientAfterCreate, res);
			cPy_Function_Execute(g_ClientAfterCreate, 0);
		}
	}
}

void cServerNetManager_OnReceive(cUserNetManager *elem, void *data, int size, void *ctl, int ctlSize) {
	cServerNetManager *self = (cServerNetManager *)elem;
	char s[0x80];
	byte kind, n;

	if (size > 4 && (g_pApp->flags & APPF_CLIENT)) {
		cFileMem_SetBuffer((&g_NetMsg), data, size, 0, 0);
		cFileMem_Open(&g_NetMsg.base, "", FILE_MODE_READ);
		ServerNet_ReadEntities(self, size);
		cFileMem_Close(&g_NetMsg.base);
	}
	if (ctlSize <= 0) {
		return;
	}
	cFileMem_SetBuffer((&g_NetMsg), ctl, ctlSize, 0, 0);
	cFileMem_Open(&g_NetMsg.base, "", FILE_MODE_READ);
	g_NetMsg.base.vftable->Read(&g_NetMsg.base, &kind, 1);
	switch (kind) {
	case NETMSG_LEVEL:
		cAbstractFile_ReadString8(&g_NetMsg.base, s, sizeof(s));
		CWinApp_LoadLevel(g_pApp, s);
		cAbstractFile_ReadString8(&g_NetMsg.base, s, sizeof(s));
		cString_Set(&g_ServerType, s);
		cFileMem_Close(&g_NetMsg.base);
		return;
	case NETMSG_RESOURCES:
		NetData_ReadResources(&g_NetMsg.base);
		cFileMem_Close(&g_NetMsg.base);
		return;
	case NETMSG_CHAT:
	case NETMSG_USR_STRING: {
		cPy_Function *f = kind == NETMSG_CHAT ? g_OnChatString : g_OnUsrString;
		cAbstractFile_ReadString8(&g_NetMsg.base, s, sizeof(s));
		cFileMem_Close(&g_NetMsg.base);
		cPy_Function_AddParamI(f, self->base.slot);
		cPy_Function_AddParamS(f, s);
		cPy_Function_Execute(f, 0);
		return;
	}
	case NETMSG_USR_DATA:
		NetData_ReadUsrDescriptors(&g_NetMsg.base);
		cFileMem_Close(&g_NetMsg.base);
		return;
	case NETMSG_GOODBYE:
		cFileMem_Close(&g_NetMsg.base);
		return;
	case NETMSG_DELETE: {
		g_NetMsg.base.vftable->Read(&g_NetMsg.base, &n, 1);
		cFileMem_Close(&g_NetMsg.base);
		Str_PrintfQuiet(s, sizeof(s), "net%d", n);
		cEntity *e = World_FindEntity(s);
		if (e && e->vftable->IsType(e, TYEN_CAR)) {
			cEntity_Del(e);
		}
		return;
	}
	case NETMSG_PING:
		cFileMem_Close(&g_NetMsg.base);
		((const cServerNetManagerVtbl *)self->base.vftable)->SendHello(&self->base, 0);
		Net_ClientReset();
		return;
	default:
		CHECK(0);
		return;
	}
}

int Net_GetClientPing(uint32_t slot) {
	if (slot > 0xff || !g_NetClientPings[slot]) {
		return 0;
	}
	return (int)pow(2.0, (double)g_NetClientPings[slot] / 24.0);
}

void cServerNetManager_SendHello(cUserNetManager *elem, byte type) {
	cServerNetManager *self = (cServerNetManager *)elem;
	uint32_t v;

	void *buf = cStaticBuffer_Get(g_NetStaticBuffer, 8000);
	cFileMem_SetBuffer((&g_NetMsg), buf, 8000, 0, 0);
	cFileMem_Open(&g_NetMsg.base, "", 6);
	g_NetMsg.base.vftable->Write(&g_NetMsg.base, &type, 1);
	v = NET_GAME_ID;
	g_NetMsg.base.vftable->Write(&g_NetMsg.base, &v, 2);
	v = NET_GAME_VERSION;
	g_NetMsg.base.vftable->Write(&g_NetMsg.base, &v, 2);
	cAbstractFile_WriteString8(&g_NetMsg.base, g_ClientPassword);
	cAbstractFile_WriteString8(&g_NetMsg.base, g_PlayerNameStr);
	cAbstractFile_WriteString8(&g_NetMsg.base, g_PlayerModel);
	v = (uint16_t)g_PlayerMaxLife;
	g_NetMsg.base.vftable->Write(&g_NetMsg.base, &v, 2);
	cAbstractFile_WriteString8(&g_NetMsg.base, g_PilotModel);
	cAbstractFile_WriteString8(&g_NetMsg.base, g_Motor0Model);
	cAbstractFile_WriteString8(&g_NetMsg.base, g_Motor1Model);
	cAbstractFile_WriteString8(&g_NetMsg.base, g_Motor2Model);
	cAbstractFile_WriteString8(&g_NetMsg.base, g_Motor3Model);
	cAbstractFile_WriteString8(&g_NetMsg.base, g_WeaponBayList);
	v = (uint32_t)g_PlayerTeamID;
	g_NetMsg.base.vftable->Write(&g_NetMsg.base, &v, 4);
	cNetClient_Push(self->base.client, buf, (uint16_t)cFileMem_Size(&g_NetMsg.base));
	cFileMem_Close(&g_NetMsg.base);
	cStaticBuffer_Unlock(g_NetStaticBuffer);
}

void cServerNetManager_Send(cUserNetManager *base, uint32_t time) {
	cServerNetManager *self = (cServerNetManager *)base;
	cEntity *e = World_FindEntity(self->name);

	if (!e) {
		cNetClient_Send(Net_GetClient(), time, 0, 0, NULL);
		return;
	}
	void *buf = cStaticBuffer_Get(g_NetStaticBuffer, 8000);
	CHECK(e->vftable->IsType(e, TYEN_CAR));
	cFileMem_SetBuffer((&g_NetMsg), buf, 8000, 0, 0);
	cFileMem_Open(&g_NetMsg.base, "", 6);
	Obj_Vtbl(e)->NetWrite(e, 0.0f, (&g_NetMsg), 0);
	int n = cFileMem_Size(&g_NetMsg.base);
	cNetClient_Send(Net_GetClient(), time, 0, n, buf);
	cFileMem_Close(&g_NetMsg.base);
	cStaticBuffer_Unlock(g_NetStaticBuffer);
}

void cUserNetManager_Nop(cUserNetManager *self) {
	(void)self;
}

/* the vtable of cServerNetManager */
const cServerNetManagerVtbl cServerNetManager_vftable = {
	.base.Delete = cServerNetManager_Delete,
	.base.OnReceive = cServerNetManager_OnReceive,
	.base.Send = cServerNetManager_Send,
	.base.SetLocalData = cUserNetManager_Nop,
	.SendHello = cServerNetManager_SendHello,
};

cGVar GVar_LogNetData;

cGVar GVar_ClientPassword;

cGVarFunction GVar_ClientAfterCreate;

cGVarFunction GVar_ClientCreate;

cGVar GVar_WeaponBayList;

cGVar GVar_Motor3Model;

cGVar GVar_PlayerMaxLife;

cGVar GVar_Motor1Model;

cGVar GVar_PilotModel;

cGVar GVar_Motor0Model;

cGVar GVar_PlayerName;

cGVar GVar_Motor2Model;

cGVar GVar_PlayerTeamID;

cGVar GVar_PlayerModel;

/* the dynamic initializers of the globals here, run in the order of src/standalone/crtxc.inc */
FINI_GVAR_STRING(Fini_GVar_WeaponBayList, &GVar_WeaponBayList)
INIT_GVAR_STRING(Init_GVar_WeaponBayList, Fini_GVar_WeaponBayList, &GVar_WeaponBayList, "WeaponBayList", 2, "List of mounted weapons(codified) A bits,B bits....\n", "1,1,0,1,3,0")
FINI_GVAR_STRING(Fini_GVar_Motor3Model, &GVar_Motor3Model)
INIT_GVAR_STRING(Init_GVar_Motor3Model, Fini_GVar_Motor3Model, &GVar_Motor3Model, "Motor3Model", 2, "The Fourth motor kind", "")
FINI_GVAR_STRING(Fini_GVar_Motor2Model, &GVar_Motor2Model)
INIT_GVAR_STRING(Init_GVar_Motor2Model, Fini_GVar_Motor2Model, &GVar_Motor2Model, "Motor2Model", 2, "The Third motor kind", "")
FINI_GVAR_STRING(Fini_GVar_Motor1Model, &GVar_Motor1Model)
INIT_GVAR_STRING(Init_GVar_Motor1Model, Fini_GVar_Motor1Model, &GVar_Motor1Model, "Motor1Model", 2, "The Second motor kind", "")
FINI_GVAR_STRING(Fini_GVar_Motor0Model, &GVar_Motor0Model)
INIT_GVAR_STRING(Init_GVar_Motor0Model, Fini_GVar_Motor0Model, &GVar_Motor0Model, "Motor0Model", 2, "The first motor kind", "MBetty1")
FINI_GVAR_STRING(Fini_GVar_PilotModel, &GVar_PilotModel)
INIT_GVAR_STRING(Init_GVar_PilotModel, Fini_GVar_PilotModel, &GVar_PilotModel, "PilotModel", 2, "Player resource", "P_Betty")
FINI_GVAR_STRING(Fini_GVar_PlayerModel, &GVar_PlayerModel)
INIT_GVAR_STRING(Init_GVar_PlayerModel, Fini_GVar_PlayerModel, &GVar_PlayerModel, "PlayerModel", 2, "Model resource", "SBetty1")
INIT_GVAR_INT(Init_GVar_PlayerMaxLife, &GVar_PlayerMaxLife, "PlayerMaxLife", 2, "The max life for the normal ship configuration...", 80)
FINI_GVAR_STRING(Fini_GVar_PlayerName, &GVar_PlayerName)
INIT_GVAR_STRING(Init_GVar_PlayerName, Fini_GVar_PlayerName, &GVar_PlayerName, "PlayerName", 2, "user's nickname", "Unnamed Player")
INIT_GVAR_INT(Init_GVar_PlayerTeamID, &GVar_PlayerTeamID, "PlayerTeamID", 6, "0:red, 2:green, 4:Killer", 0)
INIT_GVAR_INT(Init_GVar_LogNetData, &GVar_LogNetData, "LogNetData", 4, "Log client network data for debuging.\n", 0)
FINI_CALL(Fini_cFileMem_NetMsg, &g_NetMsg, cFileMem_dtor)
INIT_OBJECT_ARGS(Init_cFileMem_NetMsg, Fini_cFileMem_NetMsg, &g_NetMsg, cFileMem_ctor, 0, 0, 0)
FINI_CALL(Fini_GVar_ClientCreate, &GVar_ClientCreate.func, cPy_Function_dtor)
INIT_GVAR_FUNCTION(Init_GVar_ClientCreate, Fini_GVar_ClientCreate, &GVar_ClientCreate, "ClientCreate", 0, "ClientCreate(EntityName,ResoruceName)", "")
FINI_CALL(Fini_GVar_ClientAfterCreate, &GVar_ClientAfterCreate.func, cPy_Function_dtor)
INIT_GVAR_FUNCTION(Init_GVar_ClientAfterCreate, Fini_GVar_ClientAfterCreate, &GVar_ClientAfterCreate, "ClientAfterCreate", 0, "ClientAfterCreate(EntityName,ResoruceName)", "")
FINI_GVAR_STRING(Fini_GVar_ClientPassword, &GVar_ClientPassword)
INIT_GVAR_STRING(Init_GVar_ClientPassword, Fini_GVar_ClientPassword, &GVar_ClientPassword, "ClientPassword", 6, "The password needed to join to the server's match.\n", "sesame")
