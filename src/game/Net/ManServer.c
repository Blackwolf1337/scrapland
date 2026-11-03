/*
 * Net/ManServer.cpp of the game: cClientNetManager, the user manager of the server for one client. It makes the
 * ship of the client, reads its hello (the ship config) and its messages, and sends it the entity stream with the
 * near entities, the level, the resources and the player descriptors. See include/scrap/net.h.
 */
#include <string.h>

#include "scrap/app.h"
#include "scrap/console.h"
#include "scrap/crt.h"
#include "scrap/mathlib.h"
#include "scrap/net.h"
#include "scrap/objentity.h"
#include "scrap/vehicleentity.h"
#include "scrap/vehiclemotors.h"
#include "scrap/weapons.h"
#include "scrap/staticinit.h"

#define g_ClientNetManagerVftable (&cClientNetManager_vftable)
#define g_NetMaxResourcesSend (GVar_NetMaxResourcesSend.value.i)
#define g_ServerRemotePassword (((cString *)&GVar_ServerRemotePassword.value))
#define g_OnClientModify (((cPy_Function *)&GVar_OnClientModify.func))
static const cUserNetManagerVtbl cClientNetManager_vftable;
static cGVar GVar_ServerRemotePassword;
static cGVarFunction GVar_OnClientModify;
static cGVar GVar_NetMaxResourcesSend;

static void ClientNet_ClearConfig(cClientNetManager *self) {
	self->usrName[0] = 0;
	self->model[0] = 0;
	self->pilot[0] = 0;
	for (int m = 0; m < 4; m++) {
		self->motors[m][0] = 0;
	}
	self->weapons[0] = 0;
	self->team = 0;
}

/* the message buffer for a reliable message to this client */
static void *ClientNet_Begin(int size) {
	void *buf = cStaticBuffer_Get(g_NetStaticBuffer, size);
	cFileMem_SetBuffer((&g_NetMsg), buf, size, 0, 0);
	return buf;
}

static void cClientNetManager_InitClient(cClientNetManager *self);
cClientNetManager *cClientNetManager_ctor(cClientNetManager *self, cNetClient *client, int slot) {
	cUserNetManager_ctor(&self->base, client, slot);
	self->base.vftable = g_ClientNetManagerVftable;
	Con_Print(0x707000, "Creating new client!\n", 0);
	ClientNet_ClearConfig(self);
	self->sends = 0;
	cClientNetManager_InitClient(self);
	return self;
}

static void cClientNetManager_dtor(cClientNetManager *self) {
	self->base.vftable = g_ClientNetManagerVftable;
	cEntity *e = World_FindEntity(self->name);
	cPy_Function_AddParamS(g_OnClientExit, self->name);
	cPy_Function_AddParamS(g_OnClientExit, self->model);
	cPy_Function_AddParamS(g_OnClientExit, self->usrName);
	cPy_Function_Execute(g_OnClientExit, 0);
	byte msg[2] = { NETMSG_DELETE, (byte)self->base.slot };
	NetServer_SendAllBut(msg, 2, self->base.slot);
	if (e) {
		CHECK(Entity_IsType(e, TYEN_CAR));
		cEntity_Del(e);
	} else {
		Con_Warnf("WARNING : Player without entity '%s' disconected...\n", self->name);
	}
	Con_Printf("Client %s disconected!\n", self->name);
	ClientNet_ClearConfig(self);
	cUserNetManager_dtor(&self->base);
}

static cUserNetManager *cClientNetManager_Delete(cUserNetManager *base, byte flags) {
	cClientNetManager *self = (cClientNetManager *)base;
	cClientNetManager_dtor(self);
	if (flags & 1) {
		operator_delete_sized(self, sizeof(*self));
	}
	return base;
}

/* the hello of the client: id, version, password and the ship config */
static void ClientNet_ReadHello(cClientNetManager *self) {
	char s[0x80];
	uint16_t id = 0;
	cAbstractFile *f = &g_NetMsg.base;

	VFile_Read(f, &id, 2);
	CHECK(id == NET_GAME_ID);
	VFile_Read(f, &id, 2);
	CHECK(id == NET_GAME_VERSION);
	cAbstractFile_ReadString8(f, s, sizeof(s));
	cAbstractFile_ReadString8(f, self->usrName, sizeof(self->usrName));
	cAbstractFile_ReadString8(f, self->model, sizeof(self->model));
	VFile_Read(f, &self->maxLife, 2);
	cAbstractFile_ReadString8(f, self->pilot, sizeof(self->pilot));
	for (int m = 0; m < 4; m++) {
		cAbstractFile_ReadString8(f, self->motors[m], sizeof(self->motors[m]));
	}
	cAbstractFile_ReadString8(f, self->weapons, sizeof(self->weapons));
	VFile_Read(f, &self->team, 4);
	cFileMem_Close(&g_NetMsg.base);
}

static void cClientNetManager_SendUsers(cClientNetManager *self, byte local);
static void cClientNetManager_OnReceive(cUserNetManager *elem, void *data, int size, void *ctl, int ctlSize) {
	cClientNetManager *self = (cClientNetManager *)elem;
	char s[0x80];
	byte kind;
	cEntity *e = World_FindEntity(self->name);

	if (e && size > 0) {
		cFileMem_SetBuffer((&g_NetMsg), data, size, 0, 0);
		cFileMem_Open(&g_NetMsg.base, "", FILE_MODE_READ);
		CHECK(Entity_IsType(e, TYEN_CAR));
		Obj_NetRead(e, (&g_NetMsg), 0);
		cFileMem_Close(&g_NetMsg.base);
	}
	if (ctlSize < 1) {
		return;
	}
	cFileMem_SetBuffer((&g_NetMsg), ctl, ctlSize, 0, 0);
	cFileMem_Open(&g_NetMsg.base, "", FILE_MODE_READ);
	VFile_Read(&g_NetMsg, &kind, 1);
	if (!e && kind) {
		cFileMem_Close(&g_NetMsg.base);
		return;
	}
	switch (kind) {
	case NETMSG_LEVEL:
	case NETMSG_DELETE: {
		ClientNet_ReadHello(self);
		if (kind == NETMSG_LEVEL) {
			cClientNetManager_InitClient(self);
			e = World_FindEntity(self->name);
		}
		cWithLifeEntity_SetUsrName(e, self->usrName);
		cPy_Function *f = (kind == NETMSG_DELETE || kind == NETMSG_LEVEL) ? g_OnClientJoin : g_OnClientModify;
		cPy_Function_AddParamS(f, self->name);
		cPy_Function_AddParamS(f, self->model);
		cPy_Function_AddParamI(f, self->maxLife);
		cPy_Function_AddParamS(f, self->usrName);
		cPy_Function_AddParamS(f, self->pilot);
		for (int m = 0; m < 4; m++) {
			cPy_Function_AddParamS(f, self->motors[m]);
		}
		cPy_Function_AddParamS(f, self->weapons);
		cPy_Function_AddParamI(f, self->team);
		cPy_Function_Execute(f, 0);
		cClientNetManager_SendUsers(self, 0);
		return;
	}
	case NETMSG_GOODBYE:
		cFileMem_Close(&g_NetMsg.base);
		Con_Print(0x707000, "Goodbye recived!\n", 0);
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
		cFileMem_Close(&g_NetMsg.base);
		self->fullSends = 0;
		return;
	case NETMSG_CONSOLE:
		cAbstractFile_ReadString8(&g_NetMsg.base, s, sizeof(s));
		if (*g_ServerRemotePassword && **g_ServerRemotePassword && !cString_CmpStr(g_ServerRemotePassword, s)) {
			cAbstractFile_ReadString8(&g_NetMsg.base, s, sizeof(s));
			cFileMem_Close(&g_NetMsg.base);
			Console_Execute(s);
			return;
		}
		cFileMem_Close(&g_NetMsg.base);
		return;
	default:
		/* the file stays open, as in the original */
		CHECK(0);
		return;
	}
}

/* creates the ship "net<slot>", sends the level and the resources */
static void cClientNetManager_SendResources(cClientNetManager *self);
static void cClientNetManager_InitClient(cClientNetManager *self) {
	self->fullSends = 0;
	crt_sprintf(self->name, "net%d", self->base.slot);
	cEntity *e = World_FindEntity(self->name);
	if (e) {
		Con_Warnf("WARNING : Entity '%s' allready exists \n", self->name);
		CHECK(Entity_IsType(e, TYEN_CAR));
	} else {
		e = cWorld_CreateEntity(g_pWorld, self->name, 0.0f, 0.0f, 0.0f, TYEN_CAR);
	}
	cVehicleEntity_NetCreate((cVehicleEntity *)e);
	Entity_SetActive(e, 0);
	CHECK(strcmp(self->name, e->name) == 0);
	byte msg = NETMSG_LEVEL;
	void *buf = ClientNet_Begin(24000);
	cFileMem_Open(&g_NetMsg.base, "", 6);
	VFile_Write(&g_NetMsg, &msg, 1);
	cAbstractFile_WriteString8(&g_NetMsg.base, g_pApp->levelPath);
	cAbstractFile_WriteString8(&g_NetMsg.base, g_ServerType);
	cNetClient_Push(self->base.client, buf, (uint16_t)cFileMem_Size(&g_NetMsg.base));
	cFileMem_Close(&g_NetMsg.base);
	cStaticBuffer_Unlock(g_NetStaticBuffer);
	cClientNetManager_SendResources(self);
}

void ManServer_SendUsrData(int slot, byte toAll) {
	byte msg = NETMSG_USR_DATA;
	void *buf = ClientNet_Begin(24000);

	cFileMem_Open(&g_NetMsg.base, "", 6);
	VFile_Write(&g_NetMsg, &msg, 1);
	NetData_WriteUsr(&g_NetMsg.base, (uint16_t)slot);
	if (toAll) {
		NetServer_SendAll(buf, (uint16_t)cFileMem_Size(&g_NetMsg.base));
	} else {
		NetServer_SendAllBut(buf, (uint16_t)cFileMem_Size(&g_NetMsg.base), slot);
	}
	cFileMem_Close(&g_NetMsg.base);
	cStaticBuffer_Unlock(g_NetStaticBuffer);
}

/* runs of resource names, NetMaxResourcesSend at most and gaps of up to 16 unused numbers in one message */
static void cClientNetManager_SendResources(cClientNetManager *self) {
	void *buf = ClientNet_Begin(24000);
	int i = 0;

	do {
		int first = -1, last = -1, gap = 0;
		if (i >= NET_RESOURCES) {
			break;
		}
		do {
			if (last - first >= g_NetMaxResourcesSend) {
				break;
			}
			if (Net_GetResourceName(i)) {
				last = i;
				if (first == -1) {
					first = i;
				}
			} else if (first != -1 && ++gap > 16) {
				break;
			}
			i++;
		} while (i < NET_RESOURCES);
		if (first != -1) {
			byte msg = NETMSG_RESOURCES;
			cFileMem_Open(&g_NetMsg.base, "", 6);
			VFile_Write(&g_NetMsg, &msg, 1);
			NetData_WriteResources(&g_NetMsg.base, (uint16_t)first, (byte)((byte)last - (byte)first + 1));
			cNetClient_Push(self->base.client, buf, (uint16_t)cFileMem_Size(&g_NetMsg.base));
			cFileMem_Close(&g_NetMsg.base);
		}
	} while (i < NET_RESOURCES);
	cStaticBuffer_Unlock(g_NetStaticBuffer);
}

/* the ping byte of the stream: log2(ping + 1) * 24 */
static byte ClientNet_PingByte(cNetClient *c) {
	float p = (float)(double)(uint32_t)(c->ping + 1);
	double a = log((double)p);
	double b = log(2.0);
	int v = (int)(a / b * 24.0);
	if (v > 0xff) {
		v = 0xff;
	}
	return (byte)v;
}

static const float g_NetMaxDist2 = 1.21e+10f;
vec3_t g_NetOrigin;

static void cClientNetManager_Send(cUserNetManager *elem, uint32_t time) {
	cClientNetManager *self = (cClientNetManager *)elem;
	byte b[2];
	uint32_t v;
	vec3_t tmp;
	cAbstractFile *f = &g_NetMsg.base;

	void *buf = ClientNet_Begin(24000);
	cFileMem_Open(&g_NetMsg.base, "", 6);
	uint32_t old = self->fullSends;
	uint32_t count = 0, myIdx = 0xffff;
	int full = old < 5;
	if (old < 5) {
		self->fullSends = old + 1;
	}
	/* the head: my index and the count are written again at the end */
	v = 0xffff;
	VFile_Write(f, &v, 2);
	v = 0;
	VFile_Write(f, &v, 2);
	VFile_Write(f, &g_NetOrigin.x, 4);
	VFile_Write(f, &g_NetOrigin.y, 4);
	VFile_Write(f, &g_NetOrigin.z, 4);
	uint32_t idx = (uint32_t)NetServer_GetSlotByIndex((int)self->sends % Net_GetSendRate());
	if (idx <= 0xff) {
		cNetClient *c = NetServer_GetClient((int)idx);
		CHECK(c);
		byte pb = ClientNet_PingByte(c);
		b[0] = (byte)idx;
		VFile_Write(f, b, 1);
		b[0] = pb;
		VFile_Write(f, b, 1);
	} else {
		b[0] = 0;
		VFile_Write(f, b, 1);
		b[0] = 0;
		VFile_Write(f, b, 1);
		Con_Print(0x707000, "Invalid Ping\n", 0);
	}
	/* the origin of the positions: the own ship or the entity it is in */
	cEntity *me = World_FindEntity(self->name);
	const vec3_t *o = NULL;
	if (me) {
		CHECK(Entity_IsType(me, TYEN_CAR));
		if (me->active) {
			o = Entity_GetPos(me, &tmp);
		} else {
			cEntity *t = World_FindEntity(Veh(me)->target);
			if (t) {
				o = Entity_GetPos(t, &tmp);
			}
		}
	}
	if (o) {
		g_NetOrigin.x = o->x;
		g_NetOrigin.y = o->y;
		g_NetOrigin.z = o->z;
	} else {
		g_NetOrigin.z = 0.0f;
		g_NetOrigin.y = 0.0f;
		g_NetOrigin.x = 0.0f;
	}
	/* the vehicles first, then the other entities */
	byte pass = 1;
	cEntity *e = cWorld_GetFirstEntity(g_pWorld);
	while (e) {
		const char *name = e->name;
		byte isVeh = Entity_IsType(e, TYEN_CAR);
		if ((isVeh == 0) != pass && strlen(name) > 3 && name[0] == 'n' && name[1] == 'e' && name[2] == 't') {
			byte near;
			if (!me) {
				near = 0;
			} else {
				near = me == e || !strcmp(name, Veh(me)->target);
			}
			CHECK(Entity_IsType(e, TYEN_OBJECT));
			byte hdr[3];
			uint16_t id = (uint16_t)crt_atoi(name + 3);
			memcpy(hdr, &id, 2);
			hdr[2] = Obj(e)->netRes;
			if (id <= NET_PLAYER_MAX_ID) {
				CHECK(pass);
				if (me == e) {
					myIdx = (uint16_t)count;
				}
			}
			const vec3_t *pos = Entity_GetPos(e, &tmp);
			float dy = pos->y - g_NetOrigin.y;
			float dx = pos->x - g_NetOrigin.x;
			float dz = pos->z - g_NetOrigin.z;
			float d2 = dy * dy + dx * dx + dz * dz;
			if (d2 > g_NetMaxDist2 && near) {
				d2 = g_NetMaxDist2;
			}
			if (Obj_NetNeedsSend(e, d2) || old < 5) {
				cFileMem_Write(&g_NetMsg.base, hdr, 3);
				Obj_NetWrite(e, d2, (&g_NetMsg), full);
				count++;
			}
		}
		e = cEntity_Next(e);
		if (pass && !e) {
			e = cWorld_GetFirstEntity(g_pWorld);
			pass = 0;
		}
	}
	cFileMem_Seek(&g_NetMsg.base, 0);
	v = (uint16_t)myIdx;
	VFile_Write(f, &v, 2);
	v = (uint16_t)count;
	VFile_Write(f, &v, 2);
	VFile_Write(f, &g_NetOrigin.x, 4);
	VFile_Write(f, &g_NetOrigin.y, 4);
	VFile_Write(f, &g_NetOrigin.z, 4);
	if (NetServer_SendTo(time, self->base.slot, cFileMem_Size(&g_NetMsg.base), buf)) {
		self->sends++;
	}
	cFileMem_Close(&g_NetMsg.base);
	cStaticBuffer_Unlock(g_NetStaticBuffer);
}

/* the descriptors of all players to this client (unless local), then this one's to the others */
static void cClientNetManager_SendUsers(cClientNetManager *self, byte local) {
	void *buf = ClientNet_Begin(24000);
	cEntity *e = cWorld_GetFirstEntity(g_pWorld);

	if (!local) {
		while (e) {
			byte msg = NETMSG_USR_DATA;
			cFileMem_Open(&g_NetMsg.base, "", 6);
			VFile_Write(&g_NetMsg, &msg, 1);
			e = NetData_WriteUsrDescriptors(&g_NetMsg.base, e);
			cNetClient_Push(self->base.client, buf, (uint16_t)cFileMem_Size(&g_NetMsg.base));
			cFileMem_Close(&g_NetMsg.base);
		}
	}
	cStaticBuffer_Unlock(g_NetStaticBuffer);
	ManServer_SendUsrData(self->base.slot, local);
}

static void cClientNetManager_SetLocalData(cUserNetManager *elem) {
	cClientNetManager *self = (cClientNetManager *)elem;
	cEntity *e = World_FindEntity(self->name);

	Con_Printf("SetLocalData()->%s\n", self->name);
	CHECK(e && Entity_IsType(e, TYEN_CAR));
	Str_Copy(self->model, Veh(e)->typeName, sizeof(self->model));
	self->maxLife = (uint16_t)Veh(e)->maxLife;
	Str_Copy(self->usrName, WLife(e)->usrName ? WLife(e)->usrName : "", sizeof(self->usrName));
	Str_Copy(self->pilot, Veh_GetPilot(Veh(e), 0), sizeof(self->pilot));
	for (uint32_t m = 0; m < 4; m++) {
		Str_Copy(self->motors[m], cVehicleMotors_GetMotorName(NetEnt_Motors(e), m, NULL), sizeof(self->motors[m]));
	}
	Str_Copy(self->weapons, cWeaponManager_GetWeapList(NetEnt_Weapons(e)), sizeof(self->weapons));
	self->team = WLife(e)->arrow.tgType;
	cClientNetManager_SendUsers(self, 1);
}

/* the vtable of cClientNetManager */
static const cUserNetManagerVtbl cClientNetManager_vftable = {
	.Delete = cClientNetManager_Delete,
	.OnReceive = cClientNetManager_OnReceive,
	.Send = cClientNetManager_Send,
	.SetLocalData = cClientNetManager_SetLocalData,
};

/* the engine variables (gvars) of Net/: values and their flags byte (bit 0: scripts may not write) */
static cGVar GVar_NetMaxResourcesSend;
static cGVarFunction GVar_OnClientModify;
static cGVar GVar_ServerRemotePassword;
cGVarFunction GVar_OnClientExit;

/* the dynamic initializers of the globals here, run in the order of src/standalone/crtxc.inc */
INIT_GVAR_INT(Init_GVar_NetMaxResourcesSend, &GVar_NetMaxResourcesSend, "NetMaxResourcesSend", 0, "Maximun number of net resourses send per packet at the startup.", 8)
FINI_GVAR_STRING(Fini_GVar_ServerRemotePassword, &GVar_ServerRemotePassword)
INIT_GVAR_STRING(Init_GVar_ServerRemotePassword, Fini_GVar_ServerRemotePassword, &GVar_ServerRemotePassword, "ServerRemotePassword", 6, "Password for client remote control(server). If empty, no remote control allowed.", "")
FINI_CALL(Fini_GVar_OnClientExit, &GVar_OnClientExit.func, cPy_Function_dtor)
INIT_GVAR_FUNCTION(Init_GVar_OnClientExit, Fini_GVar_OnClientExit, &GVar_OnClientExit, "OnClientExit", 0, "OnClientExit(entityname,ModelName,UsrName) player ship will be deleted", "")
FINI_CALL(Fini_GVar_OnClientModify, &GVar_OnClientModify.func, cPy_Function_dtor)
INIT_GVAR_FUNCTION(Init_GVar_OnClientModify, Fini_GVar_OnClientModify, &GVar_OnClientModify, "OnClientModify", 0, "OnClientModify(entityname,ModelName,MaxLife,UsrName,PilotName,Motor0,Motor1,Motor2,Motor3,WeaponBay,tgtype) player ship modified", "")
