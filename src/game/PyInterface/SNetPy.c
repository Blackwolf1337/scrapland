/*
 * Python module SNet (function table SNet_methods): network server, client, browser and master
 * server. The original source file is not known (no CHECK in it). Functions that need a running server give
 * None (or NULL without exception: ModifyUsrData) when there is none, as the original.
 */
#include "scrap/net.h"
#include "scrap/scriptapi.h"

/* ModifyUsrData(arg) : modifica desde el servidor los datos locales; NULL without a server (golden records it) */
API_FN(SNet, ModifyUsrData) {
	int arg;
	if (!Mode_Parse(a, n, "i", &arg) || !Net_IsServer()) {
		return 0;
	}
	return Api_Build(out, "i", Net_ModifyUsrData(arg) != 0);
}

/* ServerChangeLevel(level) */
API_FN(SNet, ServerChangeLevel) {
	const char *level;
	if (!Mode_Parse(a, n, "s", &level)) {
		return 0;
	}
	if (!Net_IsServer()) {
		return 1;
	}
	return Api_Build(out, "i", Net_ServerChangeLevel(level) != 0);
}

/* AddResource(name) */
API_FN(SNet, AddResource) {
	const char *name;
	if (!Mode_Parse(a, n, "s", &name)) {
		return 0;
	}
	if (!Net_IsServer()) {
		return 1;
	}
	return Api_Build(out, "i", Net_AddResource(name));
}

/* InitServer(name, a, b) */
API_FN(SNet, InitServer) {
	const char *name;
	int ia, ib;
	return Mode_Parse(a, n, "sii", &name, &ia, &ib) && Api_Build(out, "i", Net_InitServer(name, ia, ib, 0) != 0);
}

/* CloseServer(msg) */
API_FN(SNet, CloseServer) {
	const char *msg;
	return Mode_Parse(a, n, "s", &msg) && Api_Build(out, "i", Net_CloseServer(msg) != 0);
}

/* IsServer() : NOTA: es para depuracion, mejor use Scrap.GetNetFlags() */
API_FN(SNet, IsServer) {
	return Mode_Parse(a, n, "") && Api_Build(out, "i", Net_IsServer() != 0);
}

/* IsDedicated() : bit 2 of the application flags */
API_FN(SNet, IsDedicated) {
	return Mode_Parse(a, n, "") && Api_Build(out, "i", (int)((g_pApp->flags >> 2) & 1));
}

/* GetServerData() : (name, map, port), ("", "", 0) without a server */
API_FN(SNet, GetServerData) {
	if (!Mode_Parse(a, n, "")) {
		return 0;
	}
	if (!Net_IsServer()) {
		return Api_Build(out, "ssi", "", "", 0);
	}
	/* called in this order by the original */
	int port = Net_GetServerPort();
	const char *map = Net_GetServerMap();
	const char *name = Net_GetServerName();
	return Api_Build(out, "ssi", name, map, port);
}

/* GetBotName() */
API_FN(SNet, GetBotName) {
	char buf[128];
	if (!Mode_Parse(a, n, "")) {
		return 0;
	}
	Net_GetBotName(buf);
	return Api_Build(out, "s", buf);
}

/* GetObjName() */
API_FN(SNet, GetObjName) {
	char buf[128];
	if (!Mode_Parse(a, n, "")) {
		return 0;
	}
	Net_GetObjName(buf);
	return Api_Build(out, "s", buf);
}

/* Kick(player) */
API_FN(SNet, Kick) {
	const char *player;
	return Mode_Parse(a, n, "s", &player) && Api_Build(out, "i", Net_Kick(player) != 0);
}

/* InitBrowser([a[, address]]) : address default "255.255.255.255" */
API_FN(SNet, InitBrowser) {
	int ia = 0;
	const char *address = "255.255.255.255";
	return Mode_Parse(a, n, "|is", &ia, &address) && Api_Build(out, "i", Net_InitBrowser(ia, address));
}

/* DoneBrowser() */
API_FN(SNet, DoneBrowser) {
	return Mode_Parse(a, n, "") && Api_Build(out, "i", Net_DoneBrowser());
}

/* GetMyClientShip() : name of the own ship on a client, else 0 */
API_FN(SNet, GetMyClientShip) {
	if (!Mode_Parse(a, n, "")) {
		return 0;
	}
	if (Net_IsClient()) {
		cNetClient *client = Net_GetClient();
		if (client && client->ship) {
			return Api_Build(out, "s", client->ship->name);
		}
	}
	return Api_Build(out, "i", 0);
}

/* InitClient(address, port) */
API_FN(SNet, InitClient) {
	const char *address;
	int port;
	return Mode_Parse(a, n, "si", &address, &port) && Api_Build(out, "i", Net_InitClient(address, port) != 0);
}

/* CloseClient([msg]) */
API_FN(SNet, CloseClient) {
	const char *msg = NULL;
	if (!Mode_Parse(a, n, "|s", &msg)) {
		return 0;
	}
	if (msg) {
		return Api_Build(out, "i", Net_CloseClientMsg(msg) != 0);
	}
	return Api_Build(out, "i", Net_CloseClient() != 0);
}

/* IsClient() */
API_FN(SNet, IsClient) {
	return Mode_Parse(a, n, "") && Api_Build(out, "i", Net_IsClient() != 0);
}

/* GetClientData() : (name, port), ("", 0) without a client */
API_FN(SNet, GetClientData) {
	if (!Mode_Parse(a, n, "")) {
		return 0;
	}
	if (!Net_IsClient()) {
		return Api_Build(out, "si", "", 0);
	}
	int port = Net_GetClientPort();
	return Api_Build(out, "si", Net_GetClientName(), port);
}

/* SendUsrString(arg, text) */
API_FN(SNet, SendUsrString) {
	int arg;
	const char *text;
	return Mode_Parse(a, n, "is", &arg, &text) && Api_Build(out, "i", Net_SendUsrString(text, arg) != 0);
}

/* SendChatString(arg, text) */
API_FN(SNet, SendChatString) {
	int arg;
	const char *text;
	return Mode_Parse(a, n, "is", &arg, &text) && Api_Build(out, "i", Net_SendChatString(text, arg) != 0);
}

/* SendCtrlString(text) */
API_FN(SNet, SendCtrlString) {
	const char *text;
	return Mode_Parse(a, n, "s", &text) && Api_Build(out, "i", Net_SendCtrlString(text) != 0);
}

/* SendMasterString(text) */
API_FN(SNet, SendMasterString) {
	const char *text;
	return Mode_Parse(a, n, "s", &text) && Api_Build(out, "i", Net_SendMasterString(text));
}

/* IsMaster() : 1 si esta activado el sistema cliente */
API_FN(SNet, IsMaster) {
	return Mode_Parse(a, n, "") && Api_Build(out, "i", Net_IsMaster());
}

/* PingInetSvrs() */
API_FN(SNet, PingInetSvrs) {
	return Mode_Parse(a, n, "") && Api_Build(out, "i", Net_PingInetSvrs());
}

/* GetPing(player) */
API_FN(SNet, GetPing) {
	const char *player;
	return Mode_Parse(a, n, "s", &player) && Api_Build(out, "i", Net_GetPing(player));
}

static char str_PyDoc_SNet_AddResource[];
static char str_PyDoc_SNet_CloseClient[64];
static char str_PyDoc_SNet_CloseServer[68];
static char str_PyDoc_SNet_DoneBrowser[];
static char str_PyDoc_SNet_GetBotName[112];
static char str_PyDoc_SNet_GetClientData[];
static char str_PyDoc_SNet_GetMyClientShip[56];
static char str_PyDoc_SNet_GetObjName[];
static char str_PyDoc_SNet_GetPing[60];
static char str_PyDoc_SNet_GetServerData[68];
static char str_PyDoc_SNet_InitBrowser[];
static char str_PyDoc_SNet_InitClient[];
static char str_PyDoc_SNet_InitServer[];
static char str_PyDoc_SNet_IsClient[];
static char str_PyDoc_SNet_IsDedicated[];
static char str_PyDoc_SNet_IsMaster[];
static char str_PyDoc_SNet_IsServer[];
static char str_PyDoc_SNet_Kick[];
static char str_PyDoc_SNet_ModifyUsrData[];
static char str_PyDoc_SNet_PingInetSvrs[];
static char str_PyDoc_SNet_SendChatString[];
static char str_PyDoc_SNet_SendCtrlString[72];
static char str_PyDoc_SNet_SendMasterString[];
static char str_PyDoc_SNet_SendUsrString[104];
static char str_PyDoc_SNet_ServerChangeLevel[68];
static const portFn_t SNet_methods[] = {
	PORT_FND(InitServer, SNet_InitServer, str_PyDoc_SNet_InitServer),
	PORT_FND(CloseServer, SNet_CloseServer, str_PyDoc_SNet_CloseServer),
	PORT_FND(ServerChangeLevel, SNet_ServerChangeLevel, str_PyDoc_SNet_ServerChangeLevel),
	PORT_FND(IsServer, SNet_IsServer, str_PyDoc_SNet_IsServer),
	PORT_FND(IsDedicated, SNet_IsDedicated, str_PyDoc_SNet_IsDedicated),
	PORT_FND(GetServerData, SNet_GetServerData, str_PyDoc_SNet_GetServerData),
	PORT_FND(AddResource, SNet_AddResource, str_PyDoc_SNet_AddResource),
	PORT_FND(ModifyUsrData, SNet_ModifyUsrData, str_PyDoc_SNet_ModifyUsrData),
	PORT_FND(GetBotName, SNet_GetBotName, str_PyDoc_SNet_GetBotName),
	PORT_FND(GetObjName, SNet_GetObjName, str_PyDoc_SNet_GetObjName),
	PORT_FND(Kick, SNet_Kick, str_PyDoc_SNet_Kick),
	PORT_FND(InitClient, SNet_InitClient, str_PyDoc_SNet_InitClient),
	PORT_FND(CloseClient, SNet_CloseClient, str_PyDoc_SNet_CloseClient),
	PORT_FND(IsClient, SNet_IsClient, str_PyDoc_SNet_IsClient),
	PORT_FND(GetClientData, SNet_GetClientData, str_PyDoc_SNet_GetClientData),
	PORT_FND(GetMyClientShip, SNet_GetMyClientShip, str_PyDoc_SNet_GetMyClientShip),
	PORT_FND(InitBrowser, SNet_InitBrowser, str_PyDoc_SNet_InitBrowser),
	PORT_FND(DoneBrowser, SNet_DoneBrowser, str_PyDoc_SNet_DoneBrowser),
	PORT_FND(SendUsrString, SNet_SendUsrString, str_PyDoc_SNet_SendUsrString),
	PORT_FND(SendChatString, SNet_SendChatString, str_PyDoc_SNet_SendChatString),
	PORT_FND(SendCtrlString, SNet_SendCtrlString, str_PyDoc_SNet_SendCtrlString),
	PORT_FND(SendMasterString, SNet_SendMasterString, str_PyDoc_SNet_SendMasterString),
	PORT_FND(IsMaster, SNet_IsMaster, str_PyDoc_SNet_IsMaster),
	PORT_FND(PingInetSvrs, SNet_PingInetSvrs, str_PyDoc_SNet_PingInetSvrs),
	PORT_FND(GetPing, SNet_GetPing, str_PyDoc_SNet_GetPing),
	PORT_FNEND
};

SD_PY_MODULE(SNet, 0x42e610)


static char str_PyDoc_SNet_ModifyUsrData[] =
	"ModifyUsrData(ClientId) : \n modifica desde el servidor los datos locales.";

static char str_PyDoc_SNet_ServerChangeLevel[68] =
	"ServerChangeLevel(resource name) : carga el siguiente nivel.";

static char str_PyDoc_SNet_AddResource[] =
	"resourceid AddResource(resource name) : intenta agregar un recurso si este no existe. -1 si el pool "
	"esta lleno";

static char str_PyDoc_SNet_InitServer[] =
	"InitServer(LevelPath,MaxPlayers,ipport) : Intenta inicializar el servidor.";

static char str_PyDoc_SNet_CloseServer[68] = "CloseServer(LevelPath) : Acaba el servidor y carga un nivel.";

static char str_PyDoc_SNet_IsServer[] =
	"IsServer() : 1 si esta activado el sistema servidor \n"
	" NOTA: es para depuracion, mejor use Scrap.GetNetFlags()";

static char str_PyDoc_SNet_IsDedicated[] = "IsDedicated() : 1 si ha arrancado en modo servidor dedicado";

static char str_PyDoc_SNet_GetServerData[68] =
	"GetServerData() : Obtiene la tupla (Hostname,ipaddress,ipport)";

static char str_PyDoc_SNet_GetBotName[112] =
	"GetBotName() : Obtiene un nombre valido de entidad jugador manejada por el servidor (bot o jugador l"
	"ocal)";

static char str_PyDoc_SNet_GetObjName[] = "GetObjName() : Obtiene un nombre valido de Objeto cualesquiera.";

static char str_PyDoc_SNet_Kick[] = "Kick(entityname) : saca del servidor a un usuario no deseado.";

static char str_PyDoc_SNet_InitBrowser[] = "InitBrowser(port) : Inicializa el browser de red local.";

static char str_PyDoc_SNet_DoneBrowser[] = "DoneBrowser() : Cierra el browser de red local.";

static char str_PyDoc_SNet_GetMyClientShip[56] = "GetMyClientShip() : retorna el nombre de su nave.";

static char str_PyDoc_SNet_InitClient[] =
	"InitClient(ipAddress,ipport) : Inicia el proceso de coneccion con el servidor.";

static char str_PyDoc_SNet_CloseClient[64] = "CloseClient(LevelPath) : Acaba el cliente y carga un nivel.";

static char str_PyDoc_SNet_IsClient[] =
	"IsClient() : 1 si esta activado el sistema cliente \n"
	"NOTA: Scrap.GetNetFlags() tiene el flag cliente activado \nsi la coneccion se hizo efectiva";

static char str_PyDoc_SNet_GetClientData[] = "GetClientData() : Obtiene la tupla (ipaddress,ipport)";

static char str_PyDoc_SNet_SendUsrString[104] =
	"SendUsrString(id,string) : -1 significa a todo el mundo en modo servidor, para cliente id se ignora";

static char str_PyDoc_SNet_SendChatString[] =
	"SendChatString(id,string) : -1 significa a todo el mundo en modo servidor, para cliente id se ignora";

static char str_PyDoc_SNet_SendCtrlString[72] =
	"SendCtrlString(string) : Envia una cadena de control a los usuarios";

static char str_PyDoc_SNet_SendMasterString[] =
	"SendMasterString(string) : envia una cadena al master. Si retorna cero no hay master.";

static char str_PyDoc_SNet_IsMaster[] =
	"IsMaster() : 1 si esta activado el sistema cliente \n"
	"NOTA: Scrap.GetNetFlags() tiene el flag cliente activado \nsi la coneccion se hizo efectiva";

static char str_PyDoc_SNet_PingInetSvrs[] =
	"PingInetSvrs() : 1 exitoso. revisa el estado de los servidores en internet. Se realiza despues de br"
	"owse.";

static char str_PyDoc_SNet_GetPing[60] = "GetPing(EntityName) : Devuelve el ping de un cliente.";

PORTS(PyInterface_SNetPy)
