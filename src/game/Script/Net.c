/*
 * The Net module in C (scripts/net/net.py, issue #2): the map list of a server, spectating, the default client
 * callbacks and the way back to single player. The map tables (NetMaps, ModesMap, Modes), TeamGame and the callback
 * variables ClientCreate, ClientAfterCreate and ClientJoin stay variables of the module: the game modes and the net
 * weapons replace the callbacks.
 */
#include "scrap/script.h"

#define K(o) Script_Keep(&r, (o))
#define FAIL() return Script_Done(&r, NULL)

/* PurgeServerMapList(): only the maps of ServerMapList the server type has */
static PyObject *Net_PurgeServerMapList(PyObject *self, PyObject *args) {
	scriptRefs_t r = { .n = 0 };
	(void)self;
	(void)args;
	PyObject *keep = K(PyList_New(0)), *text = keep ? K(Script_Ask("Scrap", "Get", "(s)", "ServerMapList")) : NULL;
	PyObject *prev = text ? K(Script_EvalIn("Net", text)) : NULL, *modes = prev ? K(Script_Global("Net", "ModesMap")) : NULL;
	PyObject *type = modes ? K(Script_Ask("Scrap", "Get", "(s)", "ServerType")) : NULL;
	PyObject *valid = type ? K(PyObject_GetItem(modes, type)) : NULL;
	if (!valid) {
		FAIL();
	}
	PyObject *a;
	int more;
	for (int i = 0; (more = Script_Next(prev, i, &a)) > 0; i++) {
		int in = PySequence_Contains(valid, a);
		int bad = in < 0 || (in && PyList_Append(keep, a) < 0);
		Py_DECREF(a);
		if (bad) {
			FAIL();
		}
	}
	PyObject *rep = more == 0 ? K(Script_Repr(keep)) : NULL;
	return Script_Done(&r, rep && Script_Try("Scrap", "Set", "(sO)", "ServerMapList", rep) ? Py_ReturnNone() : NULL);
}

/* Spectate(id, value): the camera of player id flies free and its target stops, or back */
static PyObject *Net_Spectate(PyObject *self, PyObject *args) {
	scriptRefs_t r = { .n = 0 };
	PyObject *id, *value;
	(void)self;
	if (!PyArg_ParseTuple(args, "OO", &id, &value)) {
		return NULL;
	}
	int on = Script_True(value);
	PyObject *name = on >= 0 ? K(Script_Ask("Scrap", "GetCam", "(O)", id)) : NULL, *cam = name ? K(Script_Entity(name)) : NULL;
	PyObject *target = cam ? K(PyObject_GetAttrString(cam, "MainTarget")) : NULL, *t = target ? K(Script_Entity(target)) : NULL;
	byte ok = t && Script_HSet(t, "Active", PyInt_FromLong(!on)) && Script_HSet(cam, "FreeCam", PyInt_FromLong(on));
	return Script_Done(&r, ok ? Py_ReturnNone() : NULL);
}

static PyObject *Net_ClientCreateDef(PyObject *self, PyObject *args) {
	PyObject *name, *res;
	(void)self;
	return PyArg_ParseTuple(args, "OO", &name, &res) ? Py_ReturnNone() : NULL;
}

static PyObject *Net_ClientJoinDef(PyObject *self, PyObject *args) {
	PyObject *a[11];
	(void)self;
	return PyArg_ParseTuple(args, "OOOOOOOOOOO", &a[0], &a[1], &a[2], &a[3], &a[4], &a[5], &a[6], &a[7], &a[8], &a[9],
		&a[10]) ? Py_ReturnNone() : NULL;
}

/* LoadSinglePlayer(LevelPath): a client or a server closes the game into the level, else the level loads */
static PyObject *Net_LoadSinglePlayer(PyObject *self, PyObject *args) {
	PyObject *path;
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &path)) {
		return NULL;
	}
	PyObject *client = Script_Ask("SNet", "IsClient", "()");
	int c = client ? Script_True(client) : -1, s = -1;
	Py_XDECREF(client);
	if (c == 0) {
		PyObject *server = Script_Ask("SNet", "IsServer", "()");
		s = server ? Script_True(server) : -1;
		Py_XDECREF(server);
	}
	byte ok = c > 0 ? Script_Try("SNet", "CloseClient", "(O)", path) : s > 0 ? Script_Try("SNet", "CloseServer", "(O)", path) :
		s == 0 && Script_Try("Scrap", "LoadLevel", "(O)", path);
	return ok ? Py_ReturnNone() : NULL;
}

PyMethodDef g_NetMethods[] = {
	{ "PurgeServerMapList", Net_PurgeServerMapList, 1, NULL },
	{ "Spectate", Net_Spectate, 1, NULL },
	{ "ClientCreateDef", Net_ClientCreateDef, 1, NULL },
	{ "ClientAfterCreateDef", Net_ClientCreateDef, 1, NULL },
	{ "ClientJoinDef", Net_ClientJoinDef, 1, NULL },
	{ "LoadSinglePlayer", Net_LoadSinglePlayer, 1, NULL },
	{ NULL, NULL, 0, NULL },
};
