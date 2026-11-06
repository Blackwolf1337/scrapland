/* the module files (tools/modules/gen.py): the built-in copies and their table */
static const char mod_modules_sound[] = {
#embed "../../../data/modules/sound.mod"
	, 0 };
static const char mod_modules_parking[] = {
#embed "../../../data/modules/parking.mod"
	, 0 };
static const char mod_levels_gamblinden_map[] = {
#embed "../../../data/levels/gamblinden/map.mod"
	, 0 };

static const moduleFile_t module_files[] = {
	{ NULL, "Sound", "modules/sound.mod", mod_modules_sound },
	{ NULL, "Parking", "modules/parking.mod", mod_modules_parking },
	{ "gamblinden", "Map", "levels/gamblinden/map.mod", mod_levels_gamblinden_map },
};
