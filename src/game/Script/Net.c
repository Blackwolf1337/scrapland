/*
 * The Net module in C (scripts/net/net.py, issue #2): the map list of a server, spectating, the default client
 * callbacks and the way back to single player. The map tables (NetMaps, ModesMap, Modes), TeamGame and the callback
 * variables ClientCreate, ClientAfterCreate and ClientJoin stay variables of the module: the game modes and the net
 * weapons replace the callbacks.
 *
 * Native C (docs/objlib-plan.md, stage 2): the functions call the engine in C on native values; the module has no
 * state of its own in C. ModesMap is read from the registry when the map list is purged; IsClient,
 * IsServer, Scrap.Set and Scrap.LoadLevel go through their seams (OBJLIB 7). The functions of the table have native
 * entries (MODE_FN, stage 6 batch 6f) except the two default callbacks on the shared adapter Mode_PyNone2.
 */
#include "scrap/app.h"
#include "scrap/net.h"
#include "scrap/scrap.h"
#include "scrap/script.h"
#include "scrap/scriptmodes.h"

/* Spectate(id, value): the camera of player id flies free and its target stops, or back */
static byte Net_SpectateC(const nativeVal_t *id, const nativeVal_t *value) {
	int i, on = Mode_Truth(value);
	nativeVal_t name, target;
	cEntity *cam, *t;
	if (on < 0 || !Native_ArgInt(id, &i)) {
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

/* ---- the entries and adapters of the module table (OBJLIB 7: the adapters with the arguments and the errors of the
 * Python calls) ---- */

MODE_FNANY(Net, PurgeServerMapList, Mode_PurgeServerMapList("Net"))
MODE_FN(Net, Spectate, 2, Net_SpectateC(&a[0], &a[1]))
/* ClientJoinDef: the default callback does nothing with its arguments */
MODE_FN(Net, ClientJoinDef, 11, 1)
MODE_FN(Net, LoadSinglePlayer, 1, Net_LoadSinglePlayerC(&a[0]))

PyMethodDef g_NetMethods[] = {
	{ "PurgeServerMapList", Net_PurgeServerMapList, 1, NULL },
	{ "Spectate", Net_Spectate, 1, NULL },
	/* OBJLIB 7: the two default callbacks stay the shared adapter without an entry (Mode_PyNone2): functions of one
	 * adapter compare equal (ClientCreateDef == ClientAfterCreateDef, and the other modules' functions of it) */
	{ "ClientCreateDef", Mode_PyNone2, 1, NULL },
	{ "ClientAfterCreateDef", Mode_PyNone2, 1, NULL },
	{ "ClientJoinDef", Net_ClientJoinDef, 1, NULL },
	{ "LoadSinglePlayer", Net_LoadSinglePlayer, 1, NULL },
	{ NULL, NULL, 0, NULL },
};

PORTS(Net)
