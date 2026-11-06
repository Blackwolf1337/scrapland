/*
 * The OutMap and InTraffic modules in C (scripts/vehicles/outmap.py and intraffic.py, issue #2). OutMap starts an
 * outdoor level (Init: the map limits, the spawn points, the grid and the clouds, then InitOutMap for the client, the
 * server, the split screen or the single player game) and finds free spawn points for the ships. InTraffic fills an
 * indoor level with the fake traffic of the map script.
 *
 * The module variables stay in the scripts, others read and set them: OutMap.StartPos (the spawn points, a list of
 * (position, angle)), iStartPos and SpawRes; InTraffic.N_TrafficList, M_TrafficList, H_TrafficList, their counts
 * N_NumTraffic, M_NumTraffic, H_NumTraffic and Path (the map scripts set them before LoadFakeTraffic and FakeTraffic).
 */
#include <stdarg.h>

#include "scrap/scrap.h"
#include "scrap/script.h"

/* a function of the module as its script calls it (a global of the module) */
static PyObject *Om_Own(scriptFrame_t *f, const char *module, const char *func, const char *fmt, ...) {
	PyObject *fn = Sf_Global(f, module, func);
	if (!fn) {
		return NULL;
	}
	va_list va;
	va_start(va, fmt);
	PyObject *args = Py_VaBuildValue((char *)fmt, va);
	va_end(va);
	return Sf_CallArgs(f, fn, Sf_Check(f, args != NULL) ? args : NULL);
}

/* x == None */
static int Om_IsNone(scriptFrame_t *f, PyObject *x) {
	return Sf_Compare(f, x, "==", Py_None);
}

/* `pos and Scrap.TestSphere(pos[0], pos[1], pos[2], 1200, "vr") == None`: no ship or racer near the point */
static int Om_FreePos(scriptFrame_t *f, PyObject *pos) {
	int t = Sf_True(f, pos);
	if (t <= 0) {
		return t;
	}
	return Om_IsNone(f, Sf_Ask(f, "Scrap", "TestSphere", "(OOOis)", Sf_Item(f, pos, 0), Sf_Item(f, pos, 1),
		Sf_Item(f, pos, 2), 1200, "vr"));
}

/* ---- OutMap ---- */

/* BrowseSpawnPos(): the active DM_Spawn dummies into StartPos as (position, angle + 180) */
static PyObject *OutMap_BrowseSpawnPos(PyObject *self, PyObject *args) {
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	Sf_Ask(&f, "Scrap", "StartDummySearch", "(si)", "DM_Spawn*", 1);
	while (!f.failed) {
		int mark = f.n;
		PyObject *dummy = Sf_Ask(&f, "Scrap", "NextDummySearch", "()");
		int some = Sf_Compare(&f, dummy, "!=", Py_None);
		if (some <= 0) {
			break;
		}
		if (Sf_True(&f, Sf_Item(&f, dummy, 3)) > 0) {
			/* list.append with two arguments appends the tuple in Python 1.5.2 */
			Sf_Method(&f, Sf_Global(&f, "OutMap", "StartPos"), "append", "(OO)", Sf_Item(&f, dummy, 1),
				Sf_OpI(&f, Sf_Item(&f, Sf_Item(&f, dummy, 2), 1), '+', 180));
		}
		Sf_Release(&f, mark);
	}
	return Sf_None(&f);
}

/* GetSpawnPos(): the next spawn point of StartPos, round */
static PyObject *OutMap_GetSpawnPos(PyObject *self, PyObject *args) {
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	PyObject *next = Sf_OpI(&f, Sf_Global(&f, "OutMap", "iStartPos"), '+', 1);
	int n = Sf_Len(&f, Sf_Global(&f, "OutMap", "StartPos"));
	Sf_SetGlobal(&f, "OutMap", "iStartPos", Sf_Op(&f, next, '%', n >= 0 ? Sf_Int(&f, n) : NULL));
	return Sf_Return(&f, Sf_GetItem(&f, Sf_Global(&f, "OutMap", "StartPos"), Sf_Global(&f, "OutMap", "iStartPos")));
}

/* GetFreeSpawnPos(): (position, 0), the free point nearest to the player that the player does not see */
static PyObject *OutMap_GetFreeSpawnPos(PyObject *self, PyObject *args) {
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	PyObject *usr = Sf_Ask(&f, "Scrap", "UsrEntity", "(i)", 0);
	int t = Sf_True(&f, usr);
	if (t == 0) {
		return Sf_Return(&f, Om_Own(&f, "OutMap", "GetFreeSpawnPosReal", "()"));
	}
	PyObject *pos = Om_Own(&f, "OutMap", "GetNearestFreeSpawnPos", "(O)", Sf_Attr(&f, usr, "Pos"));
	return Sf_Return(&f, Sf_Build(&f, "(Oi)", pos, 0));
}

/* GetFreeSpawnPosReal(): (position, 0), a free race point after one of the next three spawn points */
static PyObject *OutMap_GetFreeSpawnPosReal(PyObject *self, PyObject *args) {
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	for (int i = 0; i < 3 && !f.failed; i++) {
		int mark = f.n;
		PyObject *sp = Sf_Item(&f, Om_Own(&f, "OutMap", "GetSpawnPos", "()"), 0);
		PyObject *pos = Sf_Ask(&f, "SAI", "GetNextRacePoint", "(Oii)", sp, 0, 2000000);
		if (Om_FreePos(&f, pos) > 0) {
			return Sf_Return(&f, Sf_Build(&f, "(Oi)", pos, 0));
		}
		Sf_Release(&f, mark);
	}
	return Sf_Return(&f, Om_Own(&f, "OutMap", "GetFreeSpawn", "()"));
}

/* GetNearestFreeSpawnPosReal(initialPos): a free race point after initialPos (seven tries) */
static PyObject *OutMap_GetNearestFreeSpawnPosReal(PyObject *self, PyObject *args) {
	PyObject *initial;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &initial)) {
		return NULL;
	}
	for (int i = 0; i < 7 && !f.failed; i++) {
		int mark = f.n;
		PyObject *pos = Sf_Ask(&f, "SAI", "GetNextRacePoint", "(Oii)", initial, 0, 2000000);
		if (Om_FreePos(&f, pos) > 0) {
			return Sf_Return(&f, pos);
		}
		Sf_Release(&f, mark);
	}
	return Sf_Return(&f, Sf_Item(&f, Om_Own(&f, "OutMap", "GetFreeSpawn", "()"), 0));
}

/* GetFreeSpawn(): the first free spawn point (the next one when none is free); SpawRes the last TestSphere */
static PyObject *OutMap_GetFreeSpawn(PyObject *self, PyObject *args) {
	scriptFrame_t f = { 0 };
	long count = 0;
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	int n = Sf_Len(&f, Sf_Global(&f, "OutMap", "StartPos"));
	if (n >= 0) {
		Sf_Check(&f, Script_Range(Sf_Int(&f, n), &count));
	}
	for (long i = 0; i < count && !f.failed; i++) {
		int mark = f.n;
		PyObject *pos = Om_Own(&f, "OutMap", "GetSpawnPos", "()");
		PyObject *at = Sf_Item(&f, pos, 0);
		Sf_SetGlobal(&f, "OutMap", "SpawRes", Sf_Ask(&f, "Scrap", "TestSphere", "(OOOis)", Sf_Item(&f, at, 0),
			Sf_Item(&f, at, 1), Sf_Item(&f, at, 2), 1200, "vr"));
		if (Om_IsNone(&f, Sf_Global(&f, "OutMap", "SpawRes")) > 0) {
			return Sf_Return(&f, pos);
		}
		Sf_Release(&f, mark);
	}
	return Sf_Return(&f, Om_Own(&f, "OutMap", "GetSpawnPos", "()"));
}

/*
 * GetNearestFreeSpawnPos(initialPos=None) and GetFarthestFreeSpawnPos(initialPos=None): the free race point after a
 * spawn point nearest to (the player sees it) or farthest from initialPos, the player's position by default
 */
static PyObject *OutMap_SpawnPosBy(PyObject *args, PyObject *kw, byte nearest) {
	static char *names[] = { "initialPos", NULL };
	PyObject *initial = NULL;
	scriptFrame_t f = { 0 };
	if (!PyArg_ParseTupleAndKeywords(args, kw, "|O", names, &initial)) {
		return NULL;
	}
	initial = initial ? initial : Py_None;
	PyObject *usr = Sf_Ask(&f, "Scrap", "UsrEntity", "(i)", 0);
	int t = Sf_True(&f, usr);
	if (t == 0) {
		PyObject *real = Om_Own(&f, "OutMap", "GetFreeSpawnPosReal", "()");
		return Sf_Return(&f, nearest ? Sf_Item(&f, real, 0) : real);
	}
	/* the best distance and point so far are held outside the frame, which drops each turn's values */
	PyObject *best = PyFloat_FromDouble(nearest ? 2000000.0 : 0.0);
	PyObject *bestPos = Script_Ref(Py_None);
	if (Sf_True(&f, initial) == 0) {
		initial = Sf_Attr(&f, usr, "Pos");
	}
	PyObject *list = Sf_Global(&f, "OutMap", "StartPos");
	for (int i = 0; !f.failed; i++) {
		int mark = f.n;
		PyObject *elem = Sf_Next(&f, list, i);
		if (!elem) {
			break;
		}
		PyObject *pos = Sf_Ask(&f, "SAI", "GetNextRacePoint", "(Oii)", Sf_Item(&f, elem, 0), 0, 2000000);
		PyObject *hit = Sf_Ask(&f, "Scrap", "TestSphere", "(OOOis)", Sf_Item(&f, pos, 0), Sf_Item(&f, pos, 1),
			Sf_Item(&f, pos, 2), 1200, "vr");
		int ok = Om_IsNone(&f, hit);
		if (ok > 0 && nearest) {
			PyObject *res = Sf_Ask(&f, "Scrap", "TestLine", "(OOs)", Sf_Attr(&f, usr, "Pos"), pos, "");
			ok = Sf_True(&f, res);
			if (ok > 0) {
				ok = Sf_EqS(&f, Sf_Item(&f, res, 1), "");
			}
		}
		if (ok > 0) {
			PyObject *dist = Sf_Ask(&f, "SVec", "Mod", "(O)", Sf_Ask(&f, "SVec", "Sub", "(OO)", initial, pos));
			if (Sf_Compare(&f, dist, nearest ? "<" : ">", best) > 0) {
				Py_DECREF(best);
				Py_DECREF(bestPos);
				best = Script_Ref(dist);
				bestPos = Script_Ref(pos);
			}
		}
		if (!f.failed) {
			Sf_Release(&f, mark);
		}
	}
	Sf_Keep(&f, best);
	bestPos = Sf_Keep(&f, bestPos);
	if (Om_IsNone(&f, bestPos) > 0) {
		return Sf_Return(&f, Sf_Item(&f, Om_Own(&f, "OutMap", "GetFreeSpawnPosReal", "()"), 0));
	}
	return Sf_Return(&f, bestPos);
}

static PyObject *OutMap_GetNearestFreeSpawnPos(PyObject *self, PyObject *args, PyObject *kw) {
	(void)self;
	return OutMap_SpawnPosBy(args, kw, 1);
}

static PyObject *OutMap_GetFarthestFreeSpawnPos(PyObject *self, PyObject *args, PyObject *kw) {
	(void)self;
	return OutMap_SpawnPosBy(args, kw, 0);
}

/* CreatePlayers(): a test of the developers, a ship at each active DM_Spawn (the second one with AI) and the camera */
static PyObject *OutMap_CreatePlayers(PyObject *self, PyObject *args) {
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	Sf_Import(&f, "Vehicles");
	Sf_Ask(&f, "Scrap", "StartDummySearch", "(si)", "DM_Spawn*", 1);
	/* ctr is held outside the frame, which drops each turn's values */
	PyObject *ctr = PyInt_FromLong(1);
	while (!f.failed) {
		int mark = f.n;
		PyObject *dummy = Sf_Ask(&f, "Scrap", "NextDummySearch", "()");
		if (Sf_Compare(&f, dummy, "!=", Py_None) <= 0) {
			break;
		}
		if (Sf_True(&f, Sf_Item(&f, dummy, 3)) > 0) {
			PyObject *at = Sf_Item(&f, dummy, 1);
			PyObject *o1 = Sf_Ask(&f, "Scrap", "CreateEntity", "(OOOOs)", Sf_Cat(&f, "sO", "Player", Sf_Repr(&f, ctr)),
				Sf_Item(&f, at, 0), Sf_Item(&f, at, 1), Sf_Item(&f, at, 2), "Car");
			Sf_SetAttrS(&f, o1, "TgType", "killer");
			Sf_SetAttr(&f, o1, "Descriptor", Sf_Item(&f, dummy, 0));
			Sf_SetAttr(&f, o1, "Ang", Sf_Build(&f, "(iOi)", 0, Sf_OpI(&f, Sf_Item(&f, Sf_Item(&f, dummy, 2), 1), '+', 180),
				0));
			static const char *const gvars[] = { "PlayerModel", "PlayerMaxLife", "PilotModel", "Motor0Model",
				"Motor1Model", "Motor2Model", "Motor3Model", "WeaponBayList", "PlayerName" };
			PyObject *p[9];
			PyObject *name = Sf_Attr(&f, o1, "Name");
			for (int i = 0; i < 9; i++) {
				p[i] = Sf_Ask(&f, "Scrap", "Get", "(s)", gvars[i]);
			}
			if (!f.failed) {
				Sf_Ask(&f, "Vehicles", "SetProperties", "(OOOOOOOOOO)", name, p[0], p[1], p[2], p[3], p[4], p[5], p[6],
					p[7], p[8]);
			}
			if (Sf_EqI(&f, ctr, 2) > 0) {
				Sf_SetAttrI(&f, o1, "EnableAI", 1);
				Sf_SetAttrS(&f, o1, "Descriptor", "Bad boy");
			}
			PyObject *next = Sf_OpI(&f, ctr, '+', 1);
			if (next) {
				Py_DECREF(ctr);
				ctr = Script_Ref(next);
			}
		}
		if (!f.failed) {
			Sf_Release(&f, mark);
		}
	}
	Py_DECREF(ctr);
	PyObject *cam = Sf_Ask(&f, "Scrap", "CreateEntity", "(sddds)", "Player1Cam", 0.0, 0.0, 0.0, "FCam");
	PyObject *ply1 = Sf_Ask(&f, "Scrap", "GetEntity", "(s)", "Player1");
	Sf_Ask(&f, "Vehicles", "SetActiveCam", "(OOi)", Sf_Attr(&f, ply1, "Name"), Sf_Attr(&f, cam, "Name"), 0);
	Sf_Ask(&f, "SInput", "SetActionSet", "(s)", "Racer");
	return Sf_None(&f);
}

/* InitOutMap(): the players, cameras and scorers of a network client or server, the split screen or the single
 * player game with the police, the traffic, the phone cabs, the elevators and the parkings */
static PyObject *OutMap_InitOutMap(PyObject *self, PyObject *args) {
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	PyObject *net = Sf_Ask(&f, "Scrap", "GetNetFlags", "()");
	if (Sf_True(&f, Sf_Item(&f, net, 0)) > 0) {
		Sf_Import(&f, "Ffa");
		Sf_Ask(&f, "Ffa", "InitClient", "()");
		return Sf_None(&f);
	}
	Sf_Import(&f, "Accelerator");
	Sf_Ask(&f, "Accelerator", "Init", "()");
	if (Sf_Compare(&f, Sf_Ask(&f, "Scrap", "Get", "(s)", "SplitScreen"), "!=", Sf_Int(&f, 0)) > 0) {
		Sf_Import(&f, "Split");
		Sf_Ask(&f, "Split", "Init", "()");
		return Sf_None(&f);
	}
	if (Sf_True(&f, Sf_Item(&f, net, 1)) > 0) {
		Sf_Import(&f, "Ffa");
		Sf_Ask(&f, "Ffa", "InitServer", "(O)", Sf_Item(&f, net, 2));
		return Sf_None(&f);
	}
	static const char *const inits[][2] = { { "Chars", "CreateStartupPlayer" }, { "OutPolice", "Init" },
		{ "Traffic", "Init" }, { "PhoneCab", "Init" } };
	for (int i = 0; i < 4; i++) {
		Sf_Import(&f, inits[i][0]);
		Sf_Ask(&f, inits[i][0], inits[i][1], "()");
	}
	Sf_Ask(&f, "Scrap", "Set", "(ss)", "OnTaunt", "Speech.LaunchTaunt");
	Sf_Import(&f, "Elevator");
	Sf_Ask(&f, "Elevator", "PlayerCreatedElevator", "()");
	Sf_Import(&f, "Parking");
	Sf_Ask(&f, "Parking", "Init", "()");
	Sf_Import(&f, "MissionsFuncs");
	int jack = Sf_EqS(&f, Sf_Ask(&f, "Scrap", "GetSaveVar", "(s)", "JackInTheBox.Active"), "1");
	if (jack > 0) {
		PyObject *stats = Sf_Int(&f, 1);
		for (int i = 1; i <= 3 && !f.failed; i++) {
			PyObject *res = Sf_Ask(&f, "MissionsFuncs", "CrazyDealStatus", "(i)", i);
			if (Sf_Compare(&f, Sf_Item(&f, res, 1), "<", Sf_Item(&f, res, 2)) > 0) {
				stats = Sf_Int(&f, 0);
			}
		}
		Sf_Ask(&f, "Scrap", "Set", "(sO)", "SuperDealAvaliable", stats);
	} else if (jack == 0) {
		Sf_Ask(&f, "Scrap", "Set", "(si)", "SuperDealAvaliable", 0);
	}
	Sf_Ask(&f, "Scrap", "Set", "(si)", "EnergyBarActive", 0);
	return Sf_None(&f);
}

/* BrowseAtmospherePos(): the height of the atmosphere and the top of the map from their dummies */
static PyObject *OutMap_BrowseAtmospherePos(PyObject *self, PyObject *args) {
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	Sf_Ask(&f, "Scrap", "StartDummySearch", "(si)", "DM_Atmosphere", 0);
	PyObject *dummy = Sf_Ask(&f, "Scrap", "NextDummySearch", "()");
	if (Sf_True(&f, dummy) > 0) {
		Sf_Ask(&f, "Scrap", "Set", "(sO)", "AtmosphereLimit", Sf_Item(&f, Sf_Item(&f, dummy, 1), 1));
	}
	Sf_Ask(&f, "Scrap", "StartDummySearch", "(si)", "DM_MapTop", 0);
	dummy = Sf_Ask(&f, "Scrap", "NextDummySearch", "()");
	int top = Sf_True(&f, dummy);
	if (top > 0) {
		Sf_Ask(&f, "Scrap", "Set", "(sO)", "TopLimit", Sf_Item(&f, Sf_Item(&f, dummy, 1), 1));
	} else if (top == 0) {
		Sf_Ask(&f, "Scrap", "Set", "(sO)", "TopLimit", Sf_OpI(&f, Sf_Ask(&f, "Scrap", "Get", "(s)", "AtmosphereLimit"),
			'+', 5000));
	}
	return Sf_None(&f);
}

/* Init(LevelPath): the start of an outdoor level */
static PyObject *OutMap_Init(PyObject *self, PyObject *args) {
	static const struct {
		const char *var;
		int value;
	} clouds[] = { { "CameraCloudDistance", 2000 }, { "CameraCloudPlaneSize", 1000 }, { "CameraCloudThickness", 1000 },
		{ "CameraCloudAlpha", 240 }, { "CameraCloudRed", 128 }, { "CameraCloudGreen", 128 }, { "CameraCloudBlue", 140 } };
	PyObject *path;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &path)) {
		return NULL;
	}
	Sf_Import(&f, "Menu");
	Sf_Import(&f, "Init");
	Sf_Ask(&f, "Scrap", "Set", "(sO)", "FileNameMap2D", Sf_Cat(&f, "Os", path, "/2D/Map2D.tga"));
	Sf_Ask(&f, "Scrap", "Set", "(si)", "CharGrav", 3000);
	Om_Own(&f, "OutMap", "BrowseAtmospherePos", "()");
	Om_Own(&f, "OutMap", "BrowseSpawnPos", "()");
	Sf_Ask(&f, "Scrap", "Set", "(si)", "GridUseTopLimit", 1);
	Sf_Ask(&f, "Scrap", "SetGrid", "(iii)", 50000, 50000, 50000);
	Sf_Ask(&f, "Scrap", "Set", "(si)", "Max_Rad_Obj", 10000);
	for (size_t i = 0; i < sizeof(clouds) / sizeof(clouds[0]); i++) {
		Sf_Ask(&f, "Scrap", "Set", "(si)", clouds[i].var, clouds[i].value);
	}
	Om_Own(&f, "OutMap", "InitOutMap", "()");
	Sf_Import(&f, "Items");
	Sf_Ask(&f, "Items", "Init", "()");
	Sf_Print(&f, "s", "Map initialized as Outdor");
	return Sf_None(&f);
}

PyMethodDef g_OutMapMethods[] = {
	{ "BrowseSpawnPos", OutMap_BrowseSpawnPos, 1, NULL },
	{ "GetSpawnPos", OutMap_GetSpawnPos, 1, NULL },
	{ "GetFreeSpawnPos", OutMap_GetFreeSpawnPos, 1, NULL },
	{ "GetFreeSpawnPosReal", OutMap_GetFreeSpawnPosReal, 1, NULL },
	{ "GetNearestFreeSpawnPosReal", OutMap_GetNearestFreeSpawnPosReal, 1, NULL },
	{ "GetFreeSpawn", OutMap_GetFreeSpawn, 1, NULL },
	{ "GetNearestFreeSpawnPos", (PyCFunction)(void (*)(void))OutMap_GetNearestFreeSpawnPos, METH_VARARGS | METH_KEYWORDS,
		NULL },
	{ "GetFarthestFreeSpawnPos", (PyCFunction)(void (*)(void))OutMap_GetFarthestFreeSpawnPos,
		METH_VARARGS | METH_KEYWORDS, NULL },
	{ "CreatePlayers", OutMap_CreatePlayers, 1, NULL },
	{ "InitOutMap", OutMap_InitOutMap, 1, NULL },
	{ "BrowseAtmospherePos", OutMap_BrowseAtmospherePos, 1, NULL },
	{ "Init", OutMap_Init, 1, NULL },
	{ NULL, NULL, 0, NULL },
};

/* ---- InTraffic ---- */

/* GetNam(name): the model file of a traffic ship */
static PyObject *InTraffic_GetNam(PyObject *self, PyObject *args) {
	PyObject *name;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &name)) {
		return NULL;
	}
	return Sf_Return(&f, Sf_Cat(&f, "OOs", Sf_Global(&f, "InTraffic", "Path"), name, ".M3D"));
}

/* FakeWeap(): one weapon without ammunition for the traffic */
static PyObject *InTraffic_FakeWeap(PyObject *self, PyObject *args) {
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	Sf_Ask(&f, "SWeap", "InitAmmo", "(i)", 1);
	Sf_Ask(&f, "SWeap", "InitWeap", "(i)", 1);
	Sf_Ask(&f, "SWeap", "SetFWeap", "(isi)", 0, "iAmmo", 0);
	return Sf_None(&f);
}

/* LoadFakeTraffic(): the engine sound, the weapon and the models of the three traffic lists */
static PyObject *InTraffic_LoadFakeTraffic(PyObject *self, PyObject *args) {
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	Sf_Ask(&f, "SSound", "LoadSound", "(s)", "Sounds/Vehicles/Engines/EngDiesel98CC.wav");
	Om_Own(&f, "InTraffic", "FakeWeap", "()");
	PyObject *all = Sf_Op(&f, Sf_Op(&f, Sf_Global(&f, "InTraffic", "N_TrafficList"), '+',
		Sf_Global(&f, "InTraffic", "M_TrafficList")), '+', Sf_Global(&f, "InTraffic", "H_TrafficList"));
	for (int i = 0; !f.failed; i++) {
		int mark = f.n;
		PyObject *name = Sf_Next(&f, all, i);
		if (!name) {
			break;
		}
		Sf_Ask(&f, "Scrap", "Preload3DObject", "(Oiii)", Om_Own(&f, "InTraffic", "GetNam", "(O)", name), 1, 1, 1);
		if (!f.failed) {
			Sf_Release(&f, mark);
		}
	}
	return Sf_None(&f);
}

/* CreateList(TrafficList, NumTraffic, typo): NumTraffic ships FakeTraffic<i> of random models of the list, driven by
 * the AI (the names start at 0 for each list, as in the script) */
static PyObject *InTraffic_CreateList(PyObject *self, PyObject *args) {
	PyObject *list, *num, *typo;
	scriptFrame_t f = { 0 };
	long count = 0;
	(void)self;
	if (!PyArg_ParseTuple(args, "OOO", &list, &num, &typo)) {
		return NULL;
	}
	Sf_Import(&f, "Fx");
	if (!f.failed) {
		Sf_Check(&f, Script_Range(num, &count));
	}
	for (long i = 0; i < count && !f.failed; i++) {
		int mark = f.n;
		PyObject *name = Sf_Cat(&f, "sO", "FakeTraffic", Sf_ReprI(&f, i));
		PyObject *v = Sf_Ask(&f, "Scrap", "CreateEntity", "(Oiiis)", name, 0, 0, 0, "Car");
		PyObject *fn = Sf_Attr(&f, v, "SetModel");
		int n = Sf_Len(&f, list);
		PyObject *k = n >= 0 ? Sf_ToInt(&f, Sf_Float(&f, Scrap_RandRange(0.0, (double)n - 0.1))) : NULL;
		Sf_Call(&f, fn, "(O)", Sf_GetItem(&f, list, k));
		Sf_SetAttr(&f, v, "Traf_Type", typo);
		Sf_Ask(&f, "Fx", "SetUpShipMotors", "(O)", v);
		Sf_Ask(&f, "SAI", "EnableAIVehicle", "(Oidiiiiidiidd)", Sf_Attr(&f, v, "Name"), 1, 0.1, 2, 0, 1, 0, 4, 0.5, 3, 4,
			0.785, 0.25);
		Sf_Ask(&f, "SAI", "SetStateVehicle", "(iO)", 4, Sf_Attr(&f, v, "Name"));
		Sf_SetAttrI(&f, v, "Traf_Active", 1);
		Om_Own(&f, "InTraffic", "SetSound", "(O)", v);
		if (!f.failed) {
			Sf_Release(&f, mark);
		}
	}
	return Sf_None(&f);
}

/* FakeTraffic(): the light, medium and heavy traffic of the lists, with the indoor model path */
static PyObject *InTraffic_FakeTraffic(PyObject *self, PyObject *args) {
	static const char *const lists[][3] = { { "N_TrafficList", "N_NumTraffic", "Light" },
		{ "M_TrafficList", "M_NumTraffic", "Medium" }, { "H_TrafficList", "H_NumTraffic", "Heavy" } };
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	PyObject *prev = Sf_Ask(&f, "Scrap", "Get", "(s)", "ShipPathFmt");
	Sf_Ask(&f, "Scrap", "Set", "(sO)", "ShipPathFmt", Sf_Cat(&f, "Os", Sf_Global(&f, "InTraffic", "Path"), "%s.m3d"));
	for (int i = 0; i < 3; i++) {
		Om_Own(&f, "InTraffic", "CreateList", "(OOs)", Sf_Global(&f, "InTraffic", lists[i][0]),
			Sf_Global(&f, "InTraffic", lists[i][1]), lists[i][2]);
	}
	Sf_Ask(&f, "Scrap", "Set", "(sO)", "ShipPathFmt", prev);
	return Sf_None(&f);
}

/* SetSound(v): the engine loop of a traffic ship */
static PyObject *InTraffic_SetSound(PyObject *self, PyObject *args) {
	PyObject *v;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &v)) {
		return NULL;
	}
	Sf_SetAttrS(&f, v, "snd00_name", "EngDiesel98CC");
	Sf_SetAttrI(&f, v, "snd00_loop", 1);
	Sf_SetAttrI(&f, v, "snd00_ini", 1000);
	Sf_SetAttrI(&f, v, "snd00_end", 15000);
	Sf_SetAttr(&f, v, "snd00_dopp", Sf_Float(&f, 1.0));
	Sf_SetAttr(&f, v, "snd00_vol", Sf_Float(&f, 0.08));
	return Sf_None(&f);
}

PyMethodDef g_InTrafficMethods[] = {
	{ "GetNam", InTraffic_GetNam, 1, NULL },
	{ "FakeWeap", InTraffic_FakeWeap, 1, NULL },
	{ "LoadFakeTraffic", InTraffic_LoadFakeTraffic, 1, NULL },
	{ "CreateList", InTraffic_CreateList, 1, NULL },
	{ "FakeTraffic", InTraffic_FakeTraffic, 1, NULL },
	{ "SetSound", InTraffic_SetSound, 1, NULL },
	{ NULL, NULL, 0, NULL },
};
