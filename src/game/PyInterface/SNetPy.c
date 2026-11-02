/*
 * Python module SNet (method table SNet_methods, rva 0x42e610): network server, client, browser and master
 * server. The original source file is not known (no CHECK in it). Functions that need a running server give
 * None (or NULL without exception: ModifyUsrData) when there is none, as the original.
 */
#include "scrap/net.h"
#include "scrap/python.h"

/* ModifyUsrData(arg) : modifica desde el servidor los datos locales; NULL without a server */
PyObject *SNet_ModifyUsrData(PyObject *self, PyObject *args) {
	int arg;

	(void)self;
	if (!PyArg_ParseTuple(args, "i", &arg) || !Net_IsServer()) {
		return NULL;
	}
	return Py_BuildValue("i", Net_ModifyUsrData(arg) != 0);
}

/* ServerChangeLevel(level) */
PyObject *SNet_ServerChangeLevel(PyObject *self, PyObject *args) {
	char *level;

	(void)self;
	if (!PyArg_ParseTuple(args, "s", &level)) {
		return NULL;
	}
	if (!Net_IsServer()) {
		return Py_ReturnNone();
	}
	return Py_BuildValue("i", Net_ServerChangeLevel(level) != 0);
}

/* AddResource(name) */
PyObject *SNet_AddResource(PyObject *self, PyObject *args) {
	char *name;

	(void)self;
	if (!PyArg_ParseTuple(args, "s", &name)) {
		return NULL;
	}
	if (!Net_IsServer()) {
		return Py_ReturnNone();
	}
	return Py_BuildValue("i", Net_AddResource(name));
}

/* InitServer(name, a, b) */
PyObject *SNet_InitServer(PyObject *self, PyObject *args) {
	char *name;
	int a, b;

	(void)self;
	if (!PyArg_ParseTuple(args, "sii", &name, &a, &b)) {
		return NULL;
	}
	return Py_BuildValue("i", Net_InitServer(name, a, b, 0) != 0);
}

/* CloseServer(msg) */
PyObject *SNet_CloseServer(PyObject *self, PyObject *args) {
	char *msg;

	(void)self;
	if (!PyArg_ParseTuple(args, "s", &msg)) {
		return NULL;
	}
	return Py_BuildValue("i", Net_CloseServer(msg) != 0);
}

/* IsServer() : NOTA: es para depuracion, mejor use Scrap.GetNetFlags() */
PyObject *SNet_IsServer(PyObject *self, PyObject *args) {
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	return Py_BuildValue("i", Net_IsServer() != 0);
}

/* IsDedicated() : bit 2 of the application flags */
PyObject *SNet_IsDedicated(PyObject *self, PyObject *args) {
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	return Py_BuildValue("i", (int)((g_pApp->flags >> 2) & 1));
}

/* GetServerData() : (name, map, port), ("", "", 0) without a server */
PyObject *SNet_GetServerData(PyObject *self, PyObject *args) {
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	if (!Net_IsServer()) {
		return Py_BuildValue("ssi", "", "", 0);
	}
	/* called in this order by the original */
	int port = Net_GetServerPort();
	const char *map = Net_GetServerMap();
	const char *name = Net_GetServerName();
	return Py_BuildValue("ssi", name, map, port);
}

/* GetBotName() */
PyObject *SNet_GetBotName(PyObject *self, PyObject *args) {
	char buf[128];

	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	Net_GetBotName(buf);
	return Py_BuildValue("s", buf);
}

/* GetObjName() */
PyObject *SNet_GetObjName(PyObject *self, PyObject *args) {
	char buf[128];

	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	Net_GetObjName(buf);
	return Py_BuildValue("s", buf);
}

/* Kick(player) */
PyObject *SNet_Kick(PyObject *self, PyObject *args) {
	char *player;

	(void)self;
	if (!PyArg_ParseTuple(args, "s", &player)) {
		return NULL;
	}
	return Py_BuildValue("i", Net_Kick(player) != 0);
}

/* InitBrowser([a[, address]]) : address default "255.255.255.255" */
PyObject *SNet_InitBrowser(PyObject *self, PyObject *args) {
	int a = 0;
	char *address = "255.255.255.255";

	(void)self;
	if (!PyArg_ParseTuple(args, "|is", &a, &address)) {
		return NULL;
	}
	return Py_BuildValue("i", Net_InitBrowser(a, address));
}

/* DoneBrowser() */
PyObject *SNet_DoneBrowser(PyObject *self, PyObject *args) {
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	return Py_BuildValue("i", Net_DoneBrowser());
}

/* GetMyClientShip() : name of the own ship on a client, else 0 */
PyObject *SNet_GetMyClientShip(PyObject *self, PyObject *args) {
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	if (Net_IsClient()) {
		cNetClient *client = Net_GetClient();
		if (client && client->ship) {
			return Py_BuildValue("s", client->ship->name);
		}
	}
	return Py_BuildValue("i", 0);
}

/* InitClient(address, port) */
PyObject *SNet_InitClient(PyObject *self, PyObject *args) {
	char *address;
	int port;

	(void)self;
	if (!PyArg_ParseTuple(args, "si", &address, &port)) {
		return NULL;
	}
	return Py_BuildValue("i", Net_InitClient(address, port) != 0);
}

/* CloseClient([msg]) */
PyObject *SNet_CloseClient(PyObject *self, PyObject *args) {
	char *msg = NULL;

	(void)self;
	if (!PyArg_ParseTuple(args, "|s", &msg)) {
		return NULL;
	}
	if (msg) {
		return Py_BuildValue("i", Net_CloseClientMsg(msg) != 0);
	}
	return Py_BuildValue("i", Net_CloseClient() != 0);
}

/* IsClient() */
PyObject *SNet_IsClient(PyObject *self, PyObject *args) {
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	return Py_BuildValue("i", Net_IsClient() != 0);
}

/* GetClientData() : (name, port), ("", 0) without a client */
PyObject *SNet_GetClientData(PyObject *self, PyObject *args) {
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	if (!Net_IsClient()) {
		return Py_BuildValue("si", "", 0);
	}
	int port = Net_GetClientPort();
	return Py_BuildValue("si", Net_GetClientName(), port);
}

/* SendUsrString(arg, text) */
PyObject *SNet_SendUsrString(PyObject *self, PyObject *args) {
	int arg;
	char *text;

	(void)self;
	if (!PyArg_ParseTuple(args, "is", &arg, &text)) {
		return NULL;
	}
	return Py_BuildValue("i", Net_SendUsrString(text, arg) != 0);
}

/* SendChatString(arg, text) */
PyObject *SNet_SendChatString(PyObject *self, PyObject *args) {
	int arg;
	char *text;

	(void)self;
	if (!PyArg_ParseTuple(args, "is", &arg, &text)) {
		return NULL;
	}
	return Py_BuildValue("i", Net_SendChatString(text, arg) != 0);
}

/* SendCtrlString(text) */
PyObject *SNet_SendCtrlString(PyObject *self, PyObject *args) {
	char *text;

	(void)self;
	if (!PyArg_ParseTuple(args, "s", &text)) {
		return NULL;
	}
	return Py_BuildValue("i", Net_SendCtrlString(text) != 0);
}

/* SendMasterString(text) */
PyObject *SNet_SendMasterString(PyObject *self, PyObject *args) {
	char *text;

	(void)self;
	if (!PyArg_ParseTuple(args, "s", &text)) {
		return NULL;
	}
	return Py_BuildValue("i", Net_SendMasterString(text));
}

/* IsMaster() : 1 si esta activado el sistema cliente */
PyObject *SNet_IsMaster(PyObject *self, PyObject *args) {
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	return Py_BuildValue("i", Net_IsMaster());
}

/* PingInetSvrs() */
PyObject *SNet_PingInetSvrs(PyObject *self, PyObject *args) {
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	return Py_BuildValue("i", Net_PingInetSvrs());
}

/* GetPing(player) */
PyObject *SNet_GetPing(PyObject *self, PyObject *args) {
	char *player;

	(void)self;
	if (!PyArg_ParseTuple(args, "s", &player)) {
		return NULL;
	}
	return Py_BuildValue("i", Net_GetPing(player));
}

extern char str_PyDoc_SNet_AddResource[];
extern char str_PyDoc_SNet_CloseClient[64];
extern char str_PyDoc_SNet_CloseServer[68];
extern char str_PyDoc_SNet_DoneBrowser[];
extern char str_PyDoc_SNet_GetBotName[112];
extern char str_PyDoc_SNet_GetClientData[];
extern char str_PyDoc_SNet_GetMyClientShip[56];
extern char str_PyDoc_SNet_GetObjName[];
extern char str_PyDoc_SNet_GetPing[60];
extern char str_PyDoc_SNet_GetServerData[68];
extern char str_PyDoc_SNet_InitBrowser[];
extern char str_PyDoc_SNet_InitClient[];
extern char str_PyDoc_SNet_InitServer[];
extern char str_PyDoc_SNet_IsClient[];
extern char str_PyDoc_SNet_IsDedicated[];
extern char str_PyDoc_SNet_IsMaster[];
extern char str_PyDoc_SNet_IsServer[];
extern char str_PyDoc_SNet_Kick[];
extern char str_PyDoc_SNet_ModifyUsrData[];
extern char str_PyDoc_SNet_PingInetSvrs[];
extern char str_PyDoc_SNet_SendChatString[];
extern char str_PyDoc_SNet_SendCtrlString[72];
extern char str_PyDoc_SNet_SendMasterString[];
extern char str_PyDoc_SNet_SendUsrString[104];
extern char str_PyDoc_SNet_ServerChangeLevel[68];
extern const char str_PyMethod_SNet_AddResource[];
extern const char str_PyMethod_SNet_CloseClient[];
extern const char str_PyMethod_SNet_CloseServer[];
extern const char str_PyMethod_SNet_DoneBrowser[];
extern const char str_PyMethod_SNet_GetBotName[];
extern const char str_PyMethod_SNet_GetClientData[];
extern const char str_PyMethod_SNet_GetMyClientShip[];
extern const char str_PyMethod_SNet_GetObjName[];
extern const char str_PyMethod_SNet_GetPing[];
extern const char str_PyMethod_SNet_GetServerData[];
extern const char str_PyMethod_SNet_InitBrowser[];
extern const char str_PyMethod_SNet_InitClient[];
extern const char str_PyMethod_SNet_InitServer[];
extern const char str_PyMethod_SNet_IsClient[];
extern const char str_PyMethod_SNet_IsDedicated[];
extern const char str_PyMethod_SNet_IsMaster[];
extern const char str_PyMethod_SNet_IsServer[];
extern const char str_PyMethod_SNet_Kick[];
extern const char str_PyMethod_SNet_ModifyUsrData[];
extern const char str_PyMethod_SNet_PingInetSvrs[];
extern const char str_PyMethod_SNet_SendChatString[];
extern const char str_PyMethod_SNet_SendCtrlString[];
extern const char str_PyMethod_SNet_SendMasterString[];
extern const char str_PyMethod_SNet_SendUsrString[];
extern const char str_PyMethod_SNet_ServerChangeLevel[];
PyMethodDef SNet_methods[26] = {
	{
		.ml_name = (void *)str_PyMethod_SNet_InitServer,
		.ml_meth = (void *)SNet_InitServer,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_SNet_InitServer,
	},
	{
		.ml_name = (void *)str_PyMethod_SNet_CloseServer,
		.ml_meth = (void *)SNet_CloseServer,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_SNet_CloseServer,
	},
	{
		.ml_name = (void *)str_PyMethod_SNet_ServerChangeLevel,
		.ml_meth = (void *)SNet_ServerChangeLevel,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_SNet_ServerChangeLevel,
	},
	{
		.ml_name = (void *)str_PyMethod_SNet_IsServer,
		.ml_meth = (void *)SNet_IsServer,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_SNet_IsServer,
	},
	{
		.ml_name = (void *)str_PyMethod_SNet_IsDedicated,
		.ml_meth = (void *)SNet_IsDedicated,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_SNet_IsDedicated,
	},
	{
		.ml_name = (void *)str_PyMethod_SNet_GetServerData,
		.ml_meth = (void *)SNet_GetServerData,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_SNet_GetServerData,
	},
	{
		.ml_name = (void *)str_PyMethod_SNet_AddResource,
		.ml_meth = (void *)SNet_AddResource,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_SNet_AddResource,
	},
	{
		.ml_name = (void *)str_PyMethod_SNet_ModifyUsrData,
		.ml_meth = (void *)SNet_ModifyUsrData,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_SNet_ModifyUsrData,
	},
	{
		.ml_name = (void *)str_PyMethod_SNet_GetBotName,
		.ml_meth = (void *)SNet_GetBotName,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_SNet_GetBotName,
	},
	{
		.ml_name = (void *)str_PyMethod_SNet_GetObjName,
		.ml_meth = (void *)SNet_GetObjName,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_SNet_GetObjName,
	},
	{
		.ml_name = (void *)str_PyMethod_SNet_Kick,
		.ml_meth = (void *)SNet_Kick,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_SNet_Kick,
	},
	{
		.ml_name = (void *)str_PyMethod_SNet_InitClient,
		.ml_meth = (void *)SNet_InitClient,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_SNet_InitClient,
	},
	{
		.ml_name = (void *)str_PyMethod_SNet_CloseClient,
		.ml_meth = (void *)SNet_CloseClient,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_SNet_CloseClient,
	},
	{
		.ml_name = (void *)str_PyMethod_SNet_IsClient,
		.ml_meth = (void *)SNet_IsClient,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_SNet_IsClient,
	},
	{
		.ml_name = (void *)str_PyMethod_SNet_GetClientData,
		.ml_meth = (void *)SNet_GetClientData,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_SNet_GetClientData,
	},
	{
		.ml_name = (void *)str_PyMethod_SNet_GetMyClientShip,
		.ml_meth = (void *)SNet_GetMyClientShip,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_SNet_GetMyClientShip,
	},
	{
		.ml_name = (void *)str_PyMethod_SNet_InitBrowser,
		.ml_meth = (void *)SNet_InitBrowser,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_SNet_InitBrowser,
	},
	{
		.ml_name = (void *)str_PyMethod_SNet_DoneBrowser,
		.ml_meth = (void *)SNet_DoneBrowser,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_SNet_DoneBrowser,
	},
	{
		.ml_name = (void *)str_PyMethod_SNet_SendUsrString,
		.ml_meth = (void *)SNet_SendUsrString,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_SNet_SendUsrString,
	},
	{
		.ml_name = (void *)str_PyMethod_SNet_SendChatString,
		.ml_meth = (void *)SNet_SendChatString,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_SNet_SendChatString,
	},
	{
		.ml_name = (void *)str_PyMethod_SNet_SendCtrlString,
		.ml_meth = (void *)SNet_SendCtrlString,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_SNet_SendCtrlString,
	},
	{
		.ml_name = (void *)str_PyMethod_SNet_SendMasterString,
		.ml_meth = (void *)SNet_SendMasterString,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_SNet_SendMasterString,
	},
	{
		.ml_name = (void *)str_PyMethod_SNet_IsMaster,
		.ml_meth = (void *)SNet_IsMaster,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_SNet_IsMaster,
	},
	{
		.ml_name = (void *)str_PyMethod_SNet_PingInetSvrs,
		.ml_meth = (void *)SNet_PingInetSvrs,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_SNet_PingInetSvrs,
	},
	{
		.ml_name = (void *)str_PyMethod_SNet_GetPing,
		.ml_meth = (void *)SNet_GetPing,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_SNet_GetPing,
	},
};

SD_PY_MODULE(SNet, 0x42e610)

const char str_PyMethod_SNet_InitServer[] = "InitServer";
const char str_PyMethod_SNet_CloseServer[] = "CloseServer";
const char str_PyMethod_SNet_ServerChangeLevel[] = "ServerChangeLevel";
const char str_PyMethod_SNet_IsServer[] = "IsServer";
const char str_PyMethod_SNet_IsDedicated[] = "IsDedicated";
const char str_PyMethod_SNet_GetServerData[] = "GetServerData";
const char str_PyMethod_SNet_AddResource[] = "AddResource";
const char str_PyMethod_SNet_ModifyUsrData[] = "ModifyUsrData";
const char str_PyMethod_SNet_GetBotName[] = "GetBotName";
const char str_PyMethod_SNet_GetObjName[] = "GetObjName";
const char str_PyMethod_SNet_Kick[] = "Kick";
const char str_PyMethod_SNet_InitClient[] = "InitClient";
const char str_PyMethod_SNet_CloseClient[] = "CloseClient";
const char str_PyMethod_SNet_IsClient[] = "IsClient";
const char str_PyMethod_SNet_GetClientData[] = "GetClientData";
const char str_PyMethod_SNet_GetMyClientShip[] = "GetMyClientShip";
const char str_PyMethod_SNet_InitBrowser[] = "InitBrowser";
const char str_PyMethod_SNet_DoneBrowser[] = "DoneBrowser";
const char str_PyMethod_SNet_SendUsrString[] = "SendUsrString";
const char str_PyMethod_SNet_SendChatString[] = "SendChatString";
const char str_PyMethod_SNet_SendCtrlString[] = "SendCtrlString";
const char str_PyMethod_SNet_SendMasterString[] = "SendMasterString";
const char str_PyMethod_SNet_IsMaster[] = "IsMaster";
const char str_PyMethod_SNet_PingInetSvrs[] = "PingInetSvrs";
const char str_PyMethod_SNet_GetPing[] = "GetPing";

char str_PyDoc_SNet_ModifyUsrData[] =
	"ModifyUsrData(ClientId) : \n modifica desde el servidor los datos locales.";

char str_PyDoc_SNet_ServerChangeLevel[68] =
	"ServerChangeLevel(resource name) : carga el siguiente nivel.";

char str_PyDoc_SNet_AddResource[] =
	"resourceid AddResource(resource name) : intenta agregar un recurso si este no existe. -1 si el pool "
	"esta lleno";

char str_PyDoc_SNet_InitServer[] =
	"InitServer(LevelPath,MaxPlayers,ipport) : Intenta inicializar el servidor.";

char str_PyDoc_SNet_CloseServer[68] = "CloseServer(LevelPath) : Acaba el servidor y carga un nivel.";

char str_PyDoc_SNet_IsServer[] =
	"IsServer() : 1 si esta activado el sistema servidor \n"
	" NOTA: es para depuracion, mejor use Scrap.GetNetFlags()";

char str_PyDoc_SNet_IsDedicated[] = "IsDedicated() : 1 si ha arrancado en modo servidor dedicado";

char str_PyDoc_SNet_GetServerData[68] =
	"GetServerData() : Obtiene la tupla (Hostname,ipaddress,ipport)";

char str_PyDoc_SNet_GetBotName[112] =
	"GetBotName() : Obtiene un nombre valido de entidad jugador manejada por el servidor (bot o jugador l"
	"ocal)";

char str_PyDoc_SNet_GetObjName[] = "GetObjName() : Obtiene un nombre valido de Objeto cualesquiera.";

char str_PyDoc_SNet_Kick[] = "Kick(entityname) : saca del servidor a un usuario no deseado.";

char str_PyDoc_SNet_InitBrowser[] = "InitBrowser(port) : Inicializa el browser de red local.";

char str_PyDoc_SNet_DoneBrowser[] = "DoneBrowser() : Cierra el browser de red local.";

char str_PyDoc_SNet_GetMyClientShip[56] = "GetMyClientShip() : retorna el nombre de su nave.";

char str_PyDoc_SNet_InitClient[] =
	"InitClient(ipAddress,ipport) : Inicia el proceso de coneccion con el servidor.";

char str_PyDoc_SNet_CloseClient[64] = "CloseClient(LevelPath) : Acaba el cliente y carga un nivel.";

char str_PyDoc_SNet_IsClient[] =
	"IsClient() : 1 si esta activado el sistema cliente \n"
	"NOTA: Scrap.GetNetFlags() tiene el flag cliente activado \nsi la coneccion se hizo efectiva";

char str_PyDoc_SNet_GetClientData[] = "GetClientData() : Obtiene la tupla (ipaddress,ipport)";

char str_PyDoc_SNet_SendUsrString[104] =
	"SendUsrString(id,string) : -1 significa a todo el mundo en modo servidor, para cliente id se ignora";

char str_PyDoc_SNet_SendChatString[] =
	"SendChatString(id,string) : -1 significa a todo el mundo en modo servidor, para cliente id se ignora";

char str_PyDoc_SNet_SendCtrlString[72] =
	"SendCtrlString(string) : Envia una cadena de control a los usuarios";

char str_PyDoc_SNet_SendMasterString[] =
	"SendMasterString(string) : envia una cadena al master. Si retorna cero no hay master.";

char str_PyDoc_SNet_IsMaster[] =
	"IsMaster() : 1 si esta activado el sistema cliente \n"
	"NOTA: Scrap.GetNetFlags() tiene el flag cliente activado \nsi la coneccion se hizo efectiva";

char str_PyDoc_SNet_PingInetSvrs[] =
	"PingInetSvrs() : 1 exitoso. revisa el estado de los servidores en internet. Se realiza despues de br"
	"owse.";

char str_PyDoc_SNet_GetPing[60] = "GetPing(EntityName) : Devuelve el ping de un cliente.";

