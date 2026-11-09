/*
 * The InMap module in C (scripts/chars/inmap.py, issue #2): the start of an indoor map. Init(LevelPath) sets the map,
 * the grid and the sound of an interior, creates the player and the NPCs (CharsNPC.Init, still Python or its own
 * port) and starts the other systems of an interior (conversors, crazy winds, elevators, police, the Dtritus desk,
 * the news panel). Every call stops the start where the script would stop.
 *
 * Native C (docs/objlib-plan.md, stages 2 and 3): the engine calls are C; the module has no state. The calls of the
 * other ports go through their refs (PORT_CALL); Init.UpperMap stays by name (OBJLIB 5).
 */
#include <string.h>

#include "scrap/collisiongrid.h"
#include "scrap/scrap.h"
#include "scrap/script.h"
#include "scrap/scriptnative.h"
#include "scrap/world.h"

/* the start after the map name is set */
static byte InMap_Start(void) {
	GVar_SetNum("GridUseTopLimit", 0);
	/* Scrap.SetGrid(3500, 3500, 3500) */
	vec3_t grid = { 3500.0f, 3500.0f, 3500.0f };
	Grid_Set(&g_pWorld->grid, &grid);
	GVar_SetNum("Max_Rad_Obj", 1000);
	GVar_SetNum("SoundAutoplayVolume", -60.0);
	GVar_SetNum("SoundDopplerLevel", 1.0);
	GVar_SetNum("SoundDistAttenuation", 12.5);
	if (!PORT_CALL(Chars, CreateStartupPlayer, NULL, "") || !PORT_CALL(CharsNPC, Init, NULL, "")) {
		return 0;
	}
	GVar_SetNum("CameraCloudDistance", 0);
	if (!PORT_CALL(CharConversor, Init, NULL, "") || !PORT_CALL(CrazyWing, InitCrazyWing, NULL, "") ||
		!PORT_CALL(Elevator, PlayerCreatedElevator, NULL, "") || !PORT_CALL(InPolice, Init, NULL, "")) {
		return 0;
	}
	/* the mercenaries only stay friends in Gamblinden and the Junkyard (OBJLIB 5: Init.UpperMap is a variable of
	 * Init) */
	PyObject *upper = Script_Attr("Init", "UpperMap");
	if (!upper) {
		return 0;
	}
	const char *u = PyString_Check(upper) ? PyString_AsString(upper) : NULL;
	if (!u || (strcmp(u, "GAMBLINDEN") && strcmp(u, "JUNKYARD"))) {
		SaveVar_Set("MercFriends.MercenaryA_Smartie", "0");
		SaveVar_Set("MercFriends.MercenaryC_Brutus", "0");
		SaveVar_Set("MercFriends.MercenaryB_Dumber", "0");
	}
	Py_DECREF(upper);
	GVar_SetNum("CharGrav", 1960);
	if (!PORT_CALL(DTritusDesktop, Init, NULL, "") || !PORT_CALL(NewsPanel, Init, NULL, "")) {
		return 0;
	}
	GVar_SetNum("AtmosphereLimit", 1000000000);
	GVar_SetNum("TopLimit", 1000200000);
	return 1;
}

/* Init(LevelPath): Scrap.Set("FileNameMap2D", LevelPath + "/2D/Map%i.tga"), then the start */
static byte InMap_Init(const char *path) {
	char map[0x200];
	Str_Printf(map, sizeof(map), "%s/2D/Map%%i.tga", path);
	if (GVar_SetStr("FileNameMap2D", map) < 0) {
		/* Scrap.Set gives NULL for a variable of another type */
		return Native_NullResult();
	}
	return InMap_Start();
}

/* ---- the adapter of the module table (OBJLIB 7: the argument and the errors of the Python calls) ---- */

/* Init(LevelPath) of the path object */
static byte InMap_InitObj(PyObject *path) {
	/* OBJLIB 6: the imports of the function stay with the module objects until then */
	PyObject *chars = PyImport_ImportModule("Chars");
	PyObject *string = chars ? PyImport_ImportModule("string") : NULL;
	Py_XDECREF(chars);
	if (!string) {
		return 0;
	}
	Py_DECREF(string);
	if (PyString_Check(path) && PyString_Size(path) < 0x100 && strlen(PyString_AsString(path)) == (size_t)PyString_Size(path)) {
		return InMap_Init(PyString_AsString(path));
	}
	/* any other LevelPath goes the script's way: the error of + (or a value Scrap.Set takes or refuses) */
	PyObject *map = Script_PlusS(Script_Ref(path), "/2D/Map%i.tga");
	byte ok = map && PORT_CALL(Scrap, Set, NULL, "sO", "FileNameMap2D", map);
	Py_XDECREF(map);
	return ok && InMap_Start();
}

/* the entry (stage 3, the call of InitLevel through its ref): the path as the object its call through Python passes */
static byte InMap_InitPy_E(const nativeVal_t *a, int n, nativeVal_t *out) {
	(void)out;
	if (n != 1) {
		return Port_Arity(n, 1);
	}
	PyObject *path = Native_ToPy(&a[0]);
	byte ok = path && InMap_InitObj(path);
	Py_XDECREF(path);
	return ok;
}

static PyObject *InMap_InitPy(PyObject *self, PyObject *args) {
	PyObject *path;
	(void)self;
	return PyArg_ParseTuple(args, "O", &path) && InMap_InitObj(path) ? Py_ReturnNone() : NULL;
}

PyMethodDef g_InMapMethods[] = {
	{ "Init", InMap_InitPy, 1, NULL },
	{ NULL, NULL, 0, NULL },
};

PORTS(InMap)
