/*
 * World/MapFloors.cpp of the game (src/game/World/MapFloors.c): the floors of the level for the 2D map (MapFloors,
 * embedded in cWorld at 0x2f0, g_pWorld->floor).
 */
#ifndef SCRAP_MAPFLOORS_H
#define SCRAP_MAPFLOORS_H

#include "scrap/global.h"
#include "scrap/mathlib.h"

#define MAX_2D_MAPS 16
#define MAX_2D_LINKS 8
typedef struct MapLevel {
	float y;			/* the height where the floor starts, the first one -FLT_MAX */
	int nUp;
	vec3_t up[MAX_2D_LINKS];	/* the ways to the next floor up, y + 1 */
	float upAng[MAX_2D_LINKS];	/* the angle y of the dummy */
	int nDown;
	vec3_t down[MAX_2D_LINKS];	/* y - 1 */
	float downAng[MAX_2D_LINKS];
	char name[0x80];		/* behind DM_2DMapSwitch */
} MapLevel;

typedef struct MapFloors {
	int count;
	MapLevel levels[MAX_2D_MAPS];	/* sorted by y */
} MapFloors;

MapFloors *Floor_ctor(MapFloors *self);
void Floor_dtor(MapFloors *self);
/* the distance from a to b with 200000 more for each floor between them */
float Floor_Dist(MapFloors *self, vec3_t a, vec3_t b);
/* the floor at the height y: the first one above it, 1.. */
int Floor_Get(const MapFloors *self, float y);
int MapLevel_NearestDown(const MapLevel *self, const vec3_t *p);

#endif
