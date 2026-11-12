/*
 * The InMap module in C (scripts/chars/inmap.py, issue #2): the start of an indoor map. Init(LevelPath) sets the map,
 * the grid and the sound of an interior, creates the player and the NPCs (CharsNPC.Init, still Python or its own
 * port) and starts the other systems of an interior (conversors, crazy winds, elevators, police, the Dtritus desk,
 * the news panel). Every call stops the start where the script would stop.
 *
 * Native C (docs/objlib-plan.md, stages 2 and 3): the engine calls are C; the module has no state. The calls of the
 * other ports go through their refs (PORT_CALL); Init.UpperMap is read by name.
 */
#include <string.h>

#include "scrap/collisiongrid.h"
#include "scrap/scrap.h"
#include "scrap/script.h"
#include "scrap/scriptmodes.h"
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
	/* the mercenaries only stay friends in Gamblinden and the Junkyard (Init.UpperMap is a variable of
	 * Init) */
	nativeVal_t upper;
	if (!Mode_ModGet("Init", "UpperMap", &upper)) {
		return 0;
	}
	const char *u = upper.type == NV_STR ? Native_Text(&upper) : NULL;
	if (!u || (strcmp(u, "GAMBLINDEN") && strcmp(u, "JUNKYARD"))) {
		SaveVar_Set("MercFriends.MercenaryA_Smartie", "0");
		SaveVar_Set("MercFriends.MercenaryC_Brutus", "0");
		SaveVar_Set("MercFriends.MercenaryB_Dumber", "0");
	}
	Native_Free(&upper);
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

/* ---- the function of the module table ---- */

/* Init(LevelPath) of the path value */
static byte InMap_InitV(const nativeVal_t *path) {
	nativeVal_t suffix, map;
	/* the imports of the function (pack modules from the table since batch 6b) */
	if (!Mode_Import("Chars") || !Mode_Import("string")) {
		return 0;
	}
	if (path->type == NV_STR && path->n < 0x100 && strlen(Native_Text(path)) == (size_t)path->n) {
		return InMap_Init(Native_Text(path));
	}
	/* any other LevelPath goes the script's way: the error of + (OBJLIB 9: Mode_Plus), or a value Scrap.Set takes
	 * or refuses */
	Native_SetStr(&suffix, "/2D/Map%i.tga");
	byte ok = Mode_Plus(path, &suffix, &map) && PORT_CALL(Scrap, Set, NULL, "sv", "FileNameMap2D", &map);
	Native_Free(&map);
	return ok && InMap_Start();
}

MODE_FN(InMap, InitPy, 1, InMap_InitV(&a[0]))

PyMethodDef g_InMapMethods[] = {
	{ "Init", InMap_InitPy, 1, NULL },
	{ NULL, NULL, 0, NULL },
};

PORTS(InMap)
