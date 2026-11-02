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

extern PyMethodDef SNet_methods[26];
SD_PY_MODULE(SNet, 0x42e610)
