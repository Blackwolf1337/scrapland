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
static const char mod_levels_abindoor_map[] = {
#embed "../../../data/levels/abindoor/map.mod"
	, 0 };
static const char mod_levels_abindoor_mapsnd[] = {
#embed "../../../data/levels/abindoor/mapsnd.mod"
	, 0 };
static const char mod_levels_bank_map[] = {
#embed "../../../data/levels/bank/map.mod"
	, 0 };
static const char mod_levels_bank_mapsnd[] = {
#embed "../../../data/levels/bank/mapsnd.mod"
	, 0 };
static const char mod_levels_gamblinden_mapsnd[] = {
#embed "../../../data/levels/gamblinden/mapsnd.mod"
	, 0 };
static const char mod_levels_orbit_map[] = {
#embed "../../../data/levels/orbit/map.mod"
	, 0 };
static const char mod_levels_orbit_mapdecont[] = {
#embed "../../../data/levels/orbit/mapdecont.mod"
	, 0 };
static const char mod_levels_orbit_mapsnd[] = {
#embed "../../../data/levels/orbit/mapsnd.mod"
	, 0 };
static const char mod_levels_police_map[] = {
#embed "../../../data/levels/police/map.mod"
	, 0 };
static const char mod_levels_police_mapplanes[] = {
#embed "../../../data/levels/police/mapplanes.mod"
	, 0 };
static const char mod_levels_police_mapsnd[] = {
#embed "../../../data/levels/police/mapsnd.mod"
	, 0 };
static const char mod_levels_press_map[] = {
#embed "../../../data/levels/press/map.mod"
	, 0 };
static const char mod_levels_press_mapplanes[] = {
#embed "../../../data/levels/press/mapplanes.mod"
	, 0 };
static const char mod_levels_press_mapsnd[] = {
#embed "../../../data/levels/press/mapsnd.mod"
	, 0 };
static const char mod_levels_temple_map[] = {
#embed "../../../data/levels/temple/map.mod"
	, 0 };
static const char mod_levels_temple_mapextralifes[] = {
#embed "../../../data/levels/temple/mapextralifes.mod"
	, 0 };
static const char mod_levels_temple_mapsnd[] = {
#embed "../../../data/levels/temple/mapsnd.mod"
	, 0 };
static const char mod_levels_townhall_map[] = {
#embed "../../../data/levels/townhall/map.mod"
	, 0 };
static const char mod_levels_townhall_mapsnd[] = {
#embed "../../../data/levels/townhall/mapsnd.mod"
	, 0 };

static const moduleFile_t module_files[] = {
	{ NULL, "Sound", "modules/sound.mod", mod_modules_sound },
	{ NULL, "Parking", "modules/parking.mod", mod_modules_parking },
	{ "gamblinden", "Map", "levels/gamblinden/map.mod", mod_levels_gamblinden_map },
	{ "abindoor", "Map", "levels/abindoor/map.mod", mod_levels_abindoor_map },
	{ "abindoor", "MapSnd", "levels/abindoor/mapsnd.mod", mod_levels_abindoor_mapsnd },
	{ "bank", "Map", "levels/bank/map.mod", mod_levels_bank_map },
	{ "bank", "MapSnd", "levels/bank/mapsnd.mod", mod_levels_bank_mapsnd },
	{ "gamblinden", "MapSnd", "levels/gamblinden/mapsnd.mod", mod_levels_gamblinden_mapsnd },
	{ "orbit", "Map", "levels/orbit/map.mod", mod_levels_orbit_map },
	{ "orbit", "MapDecont", "levels/orbit/mapdecont.mod", mod_levels_orbit_mapdecont },
	{ "orbit", "MapSnd", "levels/orbit/mapsnd.mod", mod_levels_orbit_mapsnd },
	{ "police", "Map", "levels/police/map.mod", mod_levels_police_map },
	{ "police", "MapPlanes", "levels/police/mapplanes.mod", mod_levels_police_mapplanes },
	{ "police", "MapSnd", "levels/police/mapsnd.mod", mod_levels_police_mapsnd },
	{ "press", "Map", "levels/press/map.mod", mod_levels_press_map },
	{ "press", "MapPlanes", "levels/press/mapplanes.mod", mod_levels_press_mapplanes },
	{ "press", "MapSnd", "levels/press/mapsnd.mod", mod_levels_press_mapsnd },
	{ "temple", "Map", "levels/temple/map.mod", mod_levels_temple_map },
	{ "temple", "MapExtraLifes", "levels/temple/mapextralifes.mod", mod_levels_temple_mapextralifes },
	{ "temple", "MapSnd", "levels/temple/mapsnd.mod", mod_levels_temple_mapsnd },
	{ "townhall", "Map", "levels/townhall/map.mod", mod_levels_townhall_map },
	{ "townhall", "MapSnd", "levels/townhall/mapsnd.mod", mod_levels_townhall_mapsnd },
};
