/*
 * The mission maker of the developers (scripts/missionmaker.py of Data.packed, not run by the game): the list of
 * every mission with its level, and mss(), which sets the save variables a mission needs and loads its level. The
 * console commands map, mission and missions and the debug menu use it (src/game/Script/MissionMaker.c).
 */
#ifndef SCRAP_MISSIONMAKER_H
#define SCRAP_MISSIONMAKER_H

#include "scrap/shared.h"

typedef struct mmEntry {
	const char *level;		/* the folder under Levels/ */
	const char *mission;		/* "" for the level alone */
	const char *dealType;		/* the SuperDealType of the arena missions, else "" */
} mmEntry;

/* FileList of missionmaker.py (without the doubled Mission_GetDataPart3), by level */
extern const mmEntry g_MissionMakerList[];
extern const int g_MissionMakerCount;

/* the levels that only the network game uses (split() of missionmaker.py) */
extern const char *const g_MissionMakerSplitMaps[];
extern const int g_MissionMakerSplitCount;

/* the first entry of a mission (case is ignored), NULL when there is none */
const mmEntry *MissionMaker_Find(const char *mission);
/* the level as the list spells it, NULL for an unknown one */
const char *MissionMaker_Level(const char *level);

/* mss(MapName, MissionName="", superDealType="", Char="Dtritus"): the save variables of this session cleared and
 * set to those of a fresh game, the mission set, the level loaded after this frame; 0 on failure */
byte MissionMaker_Load(const char *level, const char *mission, const char *dealType, const char *ch);
/* split(mapname): a network level in split screen death match */
byte MissionMaker_Split(const char *map);

#endif
