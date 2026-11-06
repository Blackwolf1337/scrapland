/*
 * The levels whose scripts are a few lines, in C (issue #2): TucZone (data02.packed, the keypad to Nano), Nano
 * (Data.packed, the test map of the police boss) and ShipEdit (data02.packed, the ship editor map). Their other
 * functions are data (the files in data/levels/tuczone, nano, shipedit).
 */
#include "scrap/script.h"
#include "Levels.h"

#define MAP "Map"

/* ---- TucZone ---- */

/* Init(LevelPath): the keypad at its dummy that takes the player to Nano */
static PyObject *Tuc_Init(PyObject *self, PyObject *args) {
	PyObject *path;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &path)) {
		return NULL;
	}
	PyObject *dummy = Lv_Dummy(&f, "DM_Keypad_In", 1);
	PyObject *p = Sf_Item(&f, dummy, 1);
	PyObject *kb = Sf_Ask(&f, "Scrap", "CreateEntity", "(sOOOs)", "Elevator", Sf_Item(&f, p, 0), Sf_Item(&f, p, 1),
		Sf_Item(&f, p, 2), "WithLife");
	Sf_SetAttr(&f, kb, "Ang", Sf_Build(&f, "(iOi)", 0, Sf_OpI(&f, Sf_Item(&f, Sf_Item(&f, dummy, 2), 1), '+', 180), 0));
	Sf_SetAttrI(&f, kb, "VRadius", 60);
	Sf_SetAttrI(&f, kb, "ArrowCanUse", 1);
	Sf_SetAttrI(&f, kb, "Life", 0);
	Sf_SetAttrI(&f, kb, "ArrnOpt", 0);
	Sf_SetAttrS(&f, kb, "ArrOpt0", "TucZone:Go to Nano");
	Sf_SetAttrS(&f, kb, "UseFunc", "Map.LoadNano");
	Sf_SetAttrI(&f, kb, "ActiveCMask", 0);
	return Sf_None(&f);
}

PyMethodDef g_TucZoneMapMethods[] = {
	{ "Init", Tuc_Init, 1, NULL },
	{ NULL, NULL, 0, NULL },
};

/* ---- Nano ---- */

/* InitStatus(LevelPath): an indoor map; AfterBrowseNPC after the NPCs are browsed, the work place of the boss */
static PyObject *Nano_InitStatus(PyObject *self, PyObject *args) {
	PyObject *path;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &path)) {
		return NULL;
	}
	Sf_Import(&f, "Init");
	Sf_Import(&f, "Chars");
	Sf_Import(&f, "CharsNPC");
	Sf_SetMod(&f, "Init", "Outdoor", Sf_Int(&f, 0));
	Sf_Method(&f, Sf_Mod(&f, "CharsNPC", "AfterBrowseNPC"), "append", "(O)", Sf_Global(&f, MAP, "AfterBrowseNPC"));
	Sf_Run(&f, "Scrap", "Set", "(ss)", "BossWorkPlace", "DM_Prueba_Boss");
	return Sf_None(&f);
}

/* Init(LevelPath): the two elevators */
static PyObject *Nano_Init(PyObject *self, PyObject *args) {
	PyObject *path;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &path)) {
		return NULL;
	}
	Sf_Import(&f, "Elevator");
	Sf_Run(&f, "Elevator", "ConnectElevators", "([ss][])", "DM_Door_Elevator_00", "DM_Door_Elevator_03");
	return Sf_None(&f);
}

PyMethodDef g_NanoMapMethods[] = {
	{ "InitStatus", Nano_InitStatus, 1, NULL },
	{ "Init", Nano_Init, 1, NULL },
	{ NULL, NULL, 0, NULL },
};

/* ---- ShipEdit ---- */

/* Init(LevelPath): the camera of the editor, the ship editor menu */
static PyObject *Se_Init(PyObject *self, PyObject *args) {
	PyObject *path;
	scriptFrame_t f = { 0 };
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &path)) {
		return NULL;
	}
	Sf_Import(&f, "ShipEdit");
	PyObject *cam = Sf_Ask(&f, "Scrap", "CreateEntity", "(siiis)", "Camerita", 0, 0, 0, "Cam");
	Sf_Run(&f, "Scrap", "SetCam", "(iO)", 0, Sf_Attr(&f, cam, "Name"));
	Sf_SetAttr(&f, cam, "camPos", Sf_Build(&f, "(iii)", -6, -1243, 1451));
	Sf_SetAttr(&f, cam, "camAng", Sf_Build(&f, "(iii)", 15, -180, 0));
	Sf_SetAttrI(&f, cam, "Fov", 70);
	Sf_Run(&f, "Scrap", "Set", "(si)", "CameraCloudAlpha", 0);
	Sf_Run(&f, "ShipEdit", "EnterEditShipMenu", "(is)", 0, "");
	Sf_Run(&f, "Scrap", "SetTimeSpeed", "(i)", 1);
	Sf_Run(&f, "Scrap", "Set", "(si)", "IsMapOutDoor", 0);
	return Sf_None(&f);
}

/* MapSnd.Init(LevelPath): returns at once (the editor has no map sounds) */
static PyObject *Se_SndInit(PyObject *self, PyObject *args) {
	PyObject *path;
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &path)) {
		return NULL;
	}
	return Py_ReturnNone();
}

PyMethodDef g_ShipEditMapMethods[] = {
	{ "Init", Se_Init, 1, NULL },
	{ NULL, NULL, 0, NULL },
};

PyMethodDef g_ShipEditMapSndMethods[] = {
	{ "Init", Se_SndInit, 1, NULL },
	{ NULL, NULL, 0, NULL },
};
