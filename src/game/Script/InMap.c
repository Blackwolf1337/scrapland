/*
 * The InMap module in C (scripts/chars/inmap.py, issue #2): the start of an indoor map. Init(LevelPath) sets the map,
 * the grid and the sound of an interior, creates the player and the NPCs (CharsNPC.Init, still Python or its own
 * port) and starts the other systems of an interior (conversors, crazy winds, elevators, police, the Dtritus desk,
 * the news panel). Every call stops the start where the script would stop.
 */
#include <string.h>

#include "scrap/scrap.h"
#include "scrap/script.h"

/* Init(LevelPath) */
static PyObject *InMap_Init(PyObject *self, PyObject *args) {
	PyObject *path;
	(void)self;
	if (!PyArg_ParseTuple(args, "O", &path)) {
		return NULL;
	}
	PyObject *chars = PyImport_ImportModule("Chars");
	PyObject *string = chars ? PyImport_ImportModule("string") : NULL;
	Py_XDECREF(chars);
	if (!string) {
		return NULL;
	}
	Py_DECREF(string);
	PyObject *suffix = PyString_FromString("/2D/Map%i.tga");
	PyObject *map = suffix ? PyNumber_Add(path, suffix) : NULL;
	Py_XDECREF(suffix);
	byte ok = map && Script_Try("Scrap", "Set", "(sO)", "FileNameMap2D", map);
	Py_XDECREF(map);
	if (!ok) {
		return NULL;
	}
	GVar_SetNum("GridUseTopLimit", 0);
	if (!Script_Try("Scrap", "SetGrid", "(iii)", 3500, 3500, 3500)) {
		return NULL;
	}
	GVar_SetNum("Max_Rad_Obj", 1000);
	GVar_SetNum("SoundAutoplayVolume", -60.0);
	GVar_SetNum("SoundDopplerLevel", 1.0);
	GVar_SetNum("SoundDistAttenuation", 12.5);
	if (!Script_Try("Chars", "CreateStartupPlayer", "()") || !Script_Try("CharsNPC", "Init", "()")) {
		return NULL;
	}
	GVar_SetNum("CameraCloudDistance", 0);
	if (!Script_Try("CharConversor", "Init", "()") || !Script_Try("CrazyWing", "InitCrazyWing", "()") ||
		!Script_Try("Elevator", "PlayerCreatedElevator", "()") || !Script_Try("InPolice", "Init", "()")) {
		return NULL;
	}
	/* the mercenaries only stay friends in Gamblinden and the Junkyard */
	PyObject *upper = Script_Attr("Init", "UpperMap");
	if (!upper) {
		return NULL;
	}
	const char *u = PyString_Check(upper) ? PyString_AsString(upper) : NULL;
	if (!u || (strcmp(u, "GAMBLINDEN") && strcmp(u, "JUNKYARD"))) {
		SaveVar_Set("MercFriends.MercenaryA_Smartie", "0");
		SaveVar_Set("MercFriends.MercenaryC_Brutus", "0");
		SaveVar_Set("MercFriends.MercenaryB_Dumber", "0");
	}
	Py_DECREF(upper);
	GVar_SetNum("CharGrav", 1960);
	if (!Script_Try("DTritusDesktop", "Init", "()") || !Script_Try("NewsPanel", "Init", "()")) {
		return NULL;
	}
	GVar_SetNum("AtmosphereLimit", 1000000000);
	GVar_SetNum("TopLimit", 1000200000);
	return Py_ReturnNone();
}

PyMethodDef g_InMapMethods[] = {
	{ "Init", InMap_Init, 1, NULL },
	{ NULL, NULL, 0, NULL },
};
