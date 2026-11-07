/*
 * The Net module in C (scripts/net/net.py, issue #2): the map list of a server, spectating, the default client
 * callbacks and the way back to single player. The map tables (NetMaps, ModesMap, Modes), TeamGame and the callback
 * variables ClientCreate, ClientAfterCreate and ClientJoin stay variables of the module: the game modes and the net
 * weapons replace the callbacks.
 *
 * Native C (docs/objlib-plan.md, stage 2): the functions call the engine in C on native values; the module has no
 * state of its own in C. ModesMap is read from the module dict when the map list is purged (OBJLIB 5); IsClient,
 * IsServer, Scrap.Set and Scrap.LoadLevel go through their seams (OBJLIB 7).
 */
#include "scrap/app.h"
#include "scrap/net.h"
#include "scrap/scrap.h"
#include "scrap/script.h"
#include "scrap/scriptmodes.h"

/* Spectate(id, value): the camera of player id flies free and its target stops, or back */
static byte Net_SpectateC(const nativeVal_t *id, int on) {
	int i;
	nativeVal_t name, target;
	cEntity *cam, *t;
	if (!Native_ArgInt(id, &i)) {
		return 0;
	}
	/* cam = Scrap.GetEntity(Scrap.GetCam(id)); Scrap.GetEntity(cam.MainTarget).Active = not value;
	 * cam.FreeCam = value */
	const char *cn = cWorld_GetCamName(g_pWorld, i);
	if (cn) {
		Native_SetStr(&name, cn);
	} else {
		Native_SetNone(&name);
	}
	if (!Mode_Ent(&name, &cam) || !Mode_Get(cam, cn, "MainTarget", &target)) {
		return 0;
	}
	byte ok = Mode_Ent(&target, &t) && Mode_SetI(t, Native_Text(&target), "Active", !on) &&
		Native_EntSet(World_FindEntity(cn), cn, "FreeCam", &(nativeVal_t){ .type = NV_INT, .i = on });
	Native_Free(&target);
	return ok;
}

/* LoadSinglePlayer(LevelPath): a client or a server closes the game into the level, else the level loads */
static byte Net_LoadSinglePlayerC(const nativeVal_t *path) {
	const char *p;
	int c = Mode_IsClient(), s = c == 0 ? Mode_IsServer() : -1;
	if (c < 0 || (c == 0 && s < 0)) {
		return 0;
	}
	if (c == 0 && s == 0) {
		/* Scrap.LoadLevel(path) (OBJLIB 7: through the seam, fc-weap records it) */
		PyObject *hook;
		int seam = Native_Seam("Scrap", "LoadLevel", &hook);
		if (seam) {
			return seam > 0 && Native_HookCall(hook, NULL, "v", path);
		}
	}
	/* SNet.CloseClient(path), SNet.CloseServer(path), Scrap.LoadLevel(path): a text argument */
	if (!Native_ArgStr(path, 1, &p)) {
		return 0;
	}
	if (c > 0) {
		Net_CloseClientMsg(p);
	} else if (s > 0) {
		Net_CloseServer(p);
	} else {
		CWinApp_LoadLevel(g_pApp, p);
	}
	return 1;
}

/* ---- the adapters of the module table (OBJLIB 7: arguments and the errors of the Python calls) ---- */

static PyObject *Net_PurgeServerMapList(PyObject *self, PyObject *args) {
	(void)self;
	(void)args;
	return Mode_Result(Mode_PurgeServerMapList("Net"));
}

static PyObject *Net_Spectate(PyObject *self, PyObject *args) {
	PyObject *id, *value;
	nativeVal_t i;
	(void)self;
	if (!PyArg_ParseTuple(args, "OO", &id, &value)) {
		return NULL;
	}
	int on = PyObject_IsTrue(value);
	if (on < 0 || !Native_Arg(id, &i)) {
		return NULL;
	}
	byte ok = Net_SpectateC(&i, on);
	Native_Free(&i);
	return Mode_Result(ok);
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

static PyObject *Net_LoadSinglePlayer(PyObject *self, PyObject *args) {
	PyObject *path;
	nativeVal_t p;
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &path) || !Native_Arg(path, &p)) {
		return NULL;
	}
	byte ok = Net_LoadSinglePlayerC(&p);
	Native_Free(&p);
	return Mode_Result(ok);
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
