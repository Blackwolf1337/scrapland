/*
 * World/MapFloors.cpp of the game: the floors of the level for the 2D map (g_pWorld->floor): the dummies
 * DM_2DMapSwitch<name> give the heights where a floor starts, DM_2DMapLinkUp<name>* and DM_2DMapLinkDown<name>* the
 * points where the paths go to the next floor up and down.
 */
#include <stdio.h>

#include "scrap/console.h"
#include "scrap/crt.h"
#include "scrap/scrapworld.h"
#include "scrap/world.h"

MapFloors *__thiscall Floor_ctor(MapFloors *self) {
	WorldNodeInfo info;
	char buf[0x100];
	int i;

	for (i = 0; i < MAX_2D_MAPS; i++) {
		self->levels[i].y = 0.0f;
		self->levels[i].nDown = 0;
		self->levels[i].nUp = 0;
		self->levels[i].name[0] = 0;
	}
	self->levels[0].y = -3.4028235e38f;
	self->count = 1;
	WorldInfo_Begin("DM_2DMapSwitch*", 1);
	while (WorldInfo_Next(&info)) {
		CHECK(MAX_2D_MAPS > self->count);
		self->levels[self->count].name[0] = 0;
		self->levels[self->count].y = info.pos.y;
		/* the name behind "DM_2DMapSwitch" */
		Str_Copy(self->levels[self->count].name, info.Name + 14, sizeof(self->levels[0].name));
		self->count++;
	}
	for (i = 1; i < self->count; i++) {
		MapLevel *l = &self->levels[i];
		vec3_t ang;

		snprintf(buf, sizeof(buf), "DM_2DMapLinkUp%s*", l->name);
		WorldInfo_Begin(buf, 1);
		l->nUp = 0;
		while (WorldInfo_Next(&info)) {
			l->upAng[l->nUp] = Matrix_GetEuler(&info.m, &ang)->y;
			l->up[l->nUp].x = info.pos.x;
			l->up[l->nUp].y = l->y + 1.0f;
			l->up[l->nUp].z = info.pos.z;
			l->nUp++;
			CHECK(l->nUp <= MAX_2D_LINKS);
		}
		if (l->nUp == 0) {
			Con_Warnf("WARNING : Dummy %s not found in Map\n", buf);
		}
		snprintf(buf, sizeof(buf), "DM_2DMapLinkDown%s*", l->name);
		WorldInfo_Begin(buf, 1);
		l->nDown = 0;
		while (WorldInfo_Next(&info)) {
			l->downAng[l->nDown] = Matrix_GetEuler(&info.m, &ang)->y;
			l->down[l->nDown].x = info.pos.x;
			l->down[l->nDown].y = l->y - 1.0f;
			l->down[l->nDown].z = info.pos.z;
			l->nDown++;
			CHECK(l->nDown <= MAX_2D_LINKS);
		}
		if (l->nDown == 0) {
			Con_Warnf("WARNING : Dummy %s not found in Map\n", buf);
		}
	}
	crt_qsort(self->levels, self->count, sizeof(MapLevel), MapLevel_CompareY);
	return self;
}

void __thiscall Floor_dtor(MapFloors *self) {
	(void)self;
}

/* the distance from a to b with 200000 more for each floor between them */
float __thiscall Floor_Dist(MapFloors *self, vec3_t a, vec3_t b) {
	float dy = a.y - b.y, dx = a.x - b.x, dz = a.z - b.z;
	float d = (float)__builtin_sqrt((double)(dy * dy + dx * dx + dz * dz));
	int n = Floor_Get(self, b.y) - Floor_Get(self, a.y);

	return (float)__builtin_fabs((double)(float)n) * 200000.0f + d;
}

/* the floor at the height y: the first one above it, 1.. */
int __thiscall Floor_Get(const MapFloors *self, float y) {
	int i;

	for (i = 1; i < self->count; i++) {
		if (self->levels[i].y > y) {
			return i;
		}
	}
	return self->count;
}

static int MapLevel_Nearest(const vec3_t *links, int n, const vec3_t *p) {
	float best = 3.4028235e38f;
	int i, found = 0;

	for (i = 0; i < n; i++) {
		float dy = p->y - links[i].y, dx = p->x - links[i].x, dz = p->z - links[i].z;
		float d = dy * dy + dx * dx + dz * dz;

		if (best > d) {
			best = d;
			found = i;
		}
	}
	return found;
}

int __thiscall MapLevel_NearestDown(const MapLevel *self, const vec3_t *p) {
	return MapLevel_Nearest(self->down, self->nDown, p);
}

int __thiscall MapLevel_NearestUp(const MapLevel *self, const vec3_t *p) {
	return MapLevel_Nearest(self->up, self->nUp, p);
}

/* qsort: by the height, -1 also for the same one */
int MapLevel_CompareY(const void *a, const void *b) {
	return ((const MapLevel *)a)->y > ((const MapLevel *)b)->y ? 1 : -1;
}
