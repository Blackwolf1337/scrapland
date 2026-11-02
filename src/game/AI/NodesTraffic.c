/*
 * The traffic graph of the outdoor levels, original source AI/NodesTraffic.cpp with the instances of
 * Global/GridElements.h and Global/DList.h it uses (include/scrap/nodestraffic.h).
 */
#include <string.h>

#include "scrap/console.h"
#include "scrap/crt.h"
#include "scrap/e3dutil.h"
#include "scrap/fxelements.h"
#include "scrap/fxmisc.h"
#include "scrap/mathlib.h"
#include "scrap/nodestraffic.h"
#include "scrap/scrap.h"
#include "scrap/scrapcollision.h"
#include "scrap/scrapfont.h"
#include "scrap/scrapmain.h"
#include "scrap/scrapworld.h"


/* ---- Global/DList.h ---- */

void TrafficCell_Add(cTrafficCell *cell, const InfoTrafficNode *info) {
	cTrafficCellNode *node = DList_NewNode(cell, sizeof(*node));
	node->info = *info;
	DList_LinkFirst(cell, node);
}

void TrafficCell_Clear(cTrafficCell *cell) {
	DList_Clear(cell, sizeof(cTrafficCellNode));
}

void TrafficErrors_Clear(void *list) {
	DList_Clear(list, sizeof(cTrafficErrorNode));
}

/* ---- Global/GridElements.h ---- */

/* the point in the box: the test of the CHECKs */
static int Grid_Outside(const cGridElements *self, const vec3_t *p) {
	return p->x > self->max.x || p->x < self->min.x || p->y > self->max.y || p->y < self->min.y ||
		p->z > self->max.z || p->z < self->min.z;
}

cGridElements *cGridElements_Init(cGridElements *self, const cGridElementsVtbl *vftable, const vec3_t *max, const vec3_t *min, int nx,
	int ny, int nz) {
	self->vftable = vftable;
	CHECK((max->x >= min->x) && (max->y >= min->y) && (max->z >= min->z) && (nx > 0) && (ny > 0) && (nz > 0));
	self->max = *max;
	self->min = *min;
	self->nx = nx;
	self->ny = ny;
	self->nz = nz;
	self->cell.x = (self->max.x - self->min.x) / (float)nx;
	self->numCells = nx * ny * nz;
	self->cell.y = (self->max.y - self->min.y) / (float)ny;
	self->cell.z = (self->max.z - self->min.z) / (float)nz;
	self->cells = Mem_Alloc(self->numCells * sizeof(*self->cells));
	if (!self->cells) {
		Con_FatalErrorf("ERROR: cGridElements - Insuficiente memoria Grid.\n");
	}
	Con_Printf("GridElements Inicializado: %d bytes.\n", self->numCells << 2);
	for (int i = 0; i < self->numCells; i++) {
		self->cells[i] = NULL;
	}
	return self;
}

cGridElements *cGridElements_ctor(cGridElements *self, const vec3_t *max, const vec3_t *min, int nx,
	int ny, int nz) {
	return cGridElements_Init(self, CGRIDELEMENTS_VFTABLE, max, min, nx, ny, nz);
}

void cGridElements_FreeCells(cGridElements *self, void(*clear)(void *cell), uint32_t nodeSize) {
	for (int i = 0; i < self->numCells; i++) {
		if (!self->cells[i]) {
			continue;
		}
		clear(self->cells[i]);
		cTrafficCell *cell = self->cells[i];
		if (cell) {
			/* the destructor of the cell */
			clear(cell);
			DList_FreePool(cell, nodeSize);
			operator_delete_sized(cell, sizeof(*cell));
		}
		self->cells[i] = NULL;
	}
	Mem_Free(self->cells);
}

void cGridElements_Free(cGridElements *self) {
	cGridElements_FreeCells(self, (void(*)(void *))TrafficCell_Clear, sizeof(cTrafficCellNode));
}

cGridElements *cGridElements_Delete(cGridElements *self, byte flags) {
	self->vftable = CGRIDELEMENTS_VFTABLE;
	cGridElements_Free(self);
	if (flags & 1) {
		operator_delete_sized(self, sizeof(*self));
	}
	return self;
}

byte cGridElements_Add(cGridElements *self, const vec3_t *p, const void *data) {
	const InfoTrafficNode *info = data;
	CHECK(!((p->x > self->max.x) || (p->x < self->min.x) || (p->y > self->max.y) || (p->y < self->min.y) ||
		(p->z > self->max.z) || (p->z < self->min.z)));
	int i = self->vftable->CellIndex(self, p);
	if (!self->cells[i]) {
		cTrafficCell *cell = operator_new(sizeof(*cell));
		if (cell) {
			memset(cell, 0, sizeof(*cell));
		}
		self->cells[i] = cell;
	}
	TrafficCell_Add(self->cells[i], info);
	return 1;
}

byte cGridElements_GetCellAt(cGridElements *self, const vec3_t *p, cTrafficCell **out) {
	if (Grid_Outside(self, p)) {
		CHECK(!"!((point.x > PMax.x) || (point.x < PMin.x) || (point.y > PMax.y) || (point.y < PMin.y) || "
			"(point.z > PMax.z) || (point.z < PMin.z))");
	}
	*out = self->cells[self->vftable->CellIndex(self, p)];
	return 1;
}

byte cGridElements_GetCell(cGridElements *self, const int *x, const int *y, const int *z,
	cTrafficCell **out) {
	CHECK(!((*x >= self->nx) || (*y >= self->ny) || (*z >= self->nz)));
	*out = self->cells[(self->ny * *x + *y) * self->nz + *z];
	return 1;
}

byte cGridElements_Draw(cGridElements *self, uint32_t color) {
	vec3_t a, b;

	if (!self->numCells) {
		return 0;
	}
	/* along z, the lines of the x y corners */
	for (int i = 0; i < self->nx + 1; i++) {
		float fi = (float)i, ai = fi * 0.0f;
		for (int j = 0; j < self->ny + 1; j++) {
			float fj = (float)j, aj = fj * 0.0f;
			a.x = aj + (self->cell.x * fi + self->min.x);
			a.y = self->cell.y * fj + (ai + self->min.y);
			a.z = aj + (ai + self->min.z);
			b.x = a.x;
			b.y = a.y;
			b.z = self->max.z;
			DebugUtils_DrawLine(&a, &b, color);
		}
	}
	/* along y */
	for (int i = 0; i < self->nx + 1; i++) {
		float fi = (float)i, ai = fi * 0.0f;
		for (int j = 0; j < self->nz + 1; j++) {
			float fj = (float)j, aj = fj * 0.0f;
			a.x = aj + (self->min.x + self->cell.x * fi);
			a.y = aj + (self->min.y + ai);
			a.z = self->cell.z * fj + (self->min.z + ai);
			b.x = a.x;
			b.y = self->max.y;
			b.z = a.z;
			DebugUtils_DrawLine(&a, &b, color);
		}
	}
	/* along x */
	for (int i = 0; i < self->ny + 1; i++) {
		float fi = (float)i, ai = fi * 0.0f;
		for (int j = 0; j < self->nz + 1; j++) {
			float fj = (float)j, aj = fj * 0.0f;
			a.x = aj + (self->min.x + ai);
			a.y = aj + (self->cell.y * fi + self->min.y);
			a.z = self->cell.z * fj + (ai + self->min.z);
			b.x = self->max.x;
			b.y = a.y;
			b.z = a.z;
			DebugUtils_DrawLine(&a, &b, color);
		}
	}
	return 1;
}

byte cGridElements_CellCoords(cGridElements *self, const vec3_t *p, int *x, int *y, int *z) {
	if (Grid_Outside(self, p)) {
		CHECK(!"!((point.x > PMax.x) || (point.x < PMin.x) || (point.y > PMax.y) || (point.y < PMin.y) || "
			"(point.z > PMax.z) || (point.z < PMin.z))");
	}
	*x = (int)((p->x - self->min.x) / self->cell.x);
	if (*x >= self->nx) {
		*x = self->nx - 1;
	}
	*y = (int)((p->y - self->min.y) / self->cell.y);
	if (*y >= self->ny) {
		*y = self->ny - 1;
	}
	*z = (int)((p->z - self->min.z) / self->cell.z);
	if (*z >= self->nz) {
		*z = self->nz - 1;
	}
	return 1;
}

void cGridElements_GetDivs(cGridElements *self, int *nx, int *ny, int *nz) {
	*nx = self->nx;
	*ny = self->ny;
	*nz = self->nz;
}

vec3_t *cGridElements_GetCellSize(cGridElements *self, vec3_t *out) {
	*out = self->cell;
	return out;
}

byte cGridElements_IsInside(cGridElements *self, const vec3_t *p) {
	return !Grid_Outside(self, p);
}

int cGridElements_CellIndex(cGridElements *self, const vec3_t *p) {
	int i = ((int)((p->x - self->min.x) / self->cell.x) * self->ny + (int)((p->y - self->min.y) / self->cell.y)) *
			self->nz +
		(int)((p->z - self->min.z) / self->cell.z);
	if (i >= self->numCells) {
		return self->numCells - 1;
	}
	return i;
}

/* ---- CNodesTraffic ---- */

/* x y z made a unit vector (left as it is for the length 0); the length */
static float Traffic_Normalize(vec3_t *out, float x, float y, float z) {
	float len = (float)__builtin_sqrt((double)(y * y + x * x + z * z));

	if (len == 0.0f) {
		out->x = x;
		out->y = y;
		out->z = z;
	} else {
		float inv = 1.0f / len;
		out->x = x * inv;
		out->y = y * inv;
		out->z = z * inv;
	}
	return len;
}

/* the unit diagonal the searches step distMin along: (pos + 1) - pos, normalized */
static void Traffic_SearchDir(vec3_t *out, const vec3_t *pos) {
	Traffic_Normalize(out, (pos->x + 1.0f) - pos->x, (pos->y + 1.0f) - pos->y, (pos->z + 1.0f) - pos->z);
}

/* the box of the visibility graph (vtable slot 1 of CNodesWorld) */
static void NodesWorld_GetBox(vec3_t *min, vec3_t *max) {
	typedef void(* getBox_t)(CNodesWorld * self, vec3_t * min, vec3_t * max);
	CNodesWorld *w = g_NodesWorld;
	((getBox_t)((void *const *)w->sup.vftable)[1])(w, min, max);
}

CNodesTraffic *CNodesTraffic_ctor(void *mem) {
	CNodesTraffic *self = mem;

	self->vftable = CNODESTRAFFIC_VFTABLE;
	memset(&self->errors, 0, sizeof(self->errors));
	self->cellSize = 0.0f;
	self->numTracks = 0;
	self->numHeavy = 0;
	self->numMedium = 0;
	self->numLight = 0;
	self->numHeavyLight = 0;
	self->numMediumLight = 0;
	self->tracks = NULL;
	self->heavy = NULL;
	self->medium = NULL;
	self->light = NULL;
	self->heavyLight = NULL;
	self->mediumLight = NULL;
	self->grid = NULL;
	self->numNodes = 0;
	self->min.x = 0.0f;
	self->min.y = 0.0f;
	self->min.z = 0.0f;
	return mem;
}

void CNodesTraffic_dtor(CNodesTraffic *self) {
	self->vftable = CNODESTRAFFIC_VFTABLE;
	if (self->tracks) {
		for (int i = 0; i < self->numTracks; i++) {
			if (self->tracks[i].nodes) {
				Mem_Free(self->tracks[i].nodes);
			}
		}
		Mem_Free(self->tracks);
		self->tracks = NULL;
	}
	if (self->heavy) {
		Mem_Free(self->heavy);
	}
	if (self->medium) {
		Mem_Free(self->medium);
	}
	if (self->light) {
		Mem_Free(self->light);
	}
	if (self->heavyLight) {
		Mem_Free(self->heavyLight);
	}
	if (self->mediumLight) {
		Mem_Free(self->mediumLight);
	}
	if (self->grid) {
		self->grid->vftable->Delete(self->grid, 1);
		self->grid = NULL;
	}
	TrafficErrors_Clear(&self->errors);
	DList_FreePool(&self->errors, sizeof(cTrafficErrorNode));
}

CNodesTraffic *CNodesTraffic_Delete(CNodesTraffic *self, byte flags) {
	CNodesTraffic_dtor(self);
	if (flags & 1) {
		operator_delete_sized(self, sizeof(*self));
	}
	return self;
}

void CNodesTraffic_AddVehicle(CNodesTraffic *self, int track) {
	self->tracks[track].numVehicles++;
}

void CNodesTraffic_RemoveVehicle(CNodesTraffic *self, int track) {
	self->tracks[track].numVehicles--;
}

int CNodesTraffic_NumTracks(CNodesTraffic *self) {
	return self->numTracks;
}

int CNodesTraffic_NumByType(CNodesTraffic *self, int type) {
	switch (type) {
	case TRAFFIC_HEAVY:
		return self->numHeavy;
	case TRAFFIC_MEDIUM:
		return self->numMedium;
	case TRAFFIC_LIGHT:
		return self->numLight;
	case TRAFFIC_HEAVY_LIGHT:
		return self->numHeavyLight;
	case TRAFFIC_MEDIUM_LIGHT:
		return self->numMediumLight;
	}
	return 0;
}

int CNodesTraffic_NumNodes(CNodesTraffic *self, int track) {
	return self->tracks[track].numNodes;
}

float CNodesTraffic_GetRadius(CNodesTraffic *self, int track) {
	CHECK((track >= 0) && (track < self->numTracks));
	return self->tracks[track].radius;
}

void CNodesTraffic_Init(CNodesTraffic *self) {
	self->vftable->Load(self, 1);
}

/* the track lists by type of Load: freed, made again with the indices */
static int *Traffic_SetList(int *old, const int *src, int n, const char *msg, const char *check, int line) {
	Mem_Free(old);
	int *list = Mem_Alloc(n << 2);
	if (!list) {
		Con_Bluef("%s", msg);
		CheckFailed(check, __FILE__, __TIMESTAMP__, line);
	}
	for (int i = 0; i < n; i++) {
		list[i] = src[i];
	}
	return list;
}

/* the properties of the info section of the first dummy of a track */
static void Traffic_ReadInfo(CNodesTraffic *self, int t, int id, int lists[5][50]) {
	char density[25], type[25];
	float radiusDef, velDef;

	self->tracks[t].loop = WorldInfo_GetBool("info", "loop", 1);
	self->tracks[t].id = id;
	WorldInfo_GetText("info", "density", density, sizeof(density), "high");
	if (!strcmp(density, "high")) {
		self->tracks[t].density = TRAFFIC_DENSITY_HIGH;
	} else if (!strcmp(density, "normal")) {
		self->tracks[t].density = TRAFFIC_DENSITY_NORMAL;
	} else if (!strcmp(density, "low")) {
		self->tracks[t].density = TRAFFIC_DENSITY_LOW;
	}
	self->tracks[t].numVehicles = 0;
	WorldInfo_GetText("info", "type", type, sizeof(type), "heavy");
	if (!strcmp(type, "heavy")) {
		self->tracks[t].type = TRAFFIC_HEAVY;
		lists[0][self->numHeavy++] = t;
		radiusDef = g_HTrafficCapsule;
		velDef = 100.0f;
	} else if (!strcmp(type, "medium")) {
		self->tracks[t].type = TRAFFIC_MEDIUM;
		lists[1][self->numMedium++] = t;
		radiusDef = g_MTrafficCapsule;
		velDef = 150.0f;
	} else if (!strcmp(type, "light")) {
		self->tracks[t].type = TRAFFIC_LIGHT;
		lists[2][self->numLight++] = t;
		radiusDef = g_NTrafficCapsule;
		velDef = 300.0f;
	} else if (!strcmp(type, "heavy_light")) {
		self->tracks[t].type = TRAFFIC_HEAVY_LIGHT;
		lists[3][self->numHeavyLight++] = t;
		if (g_IsMapOutDoor) {
			radiusDef = g_HNTrafficCapsule;
			velDef = 100.0f;
		} else {
			radiusDef = g_HNTrafficCapsule * 0.5f;
			velDef = 50.0f;
		}
	} else if (!strcmp(type, "medium_light")) {
		self->tracks[t].type = TRAFFIC_MEDIUM_LIGHT;
		lists[4][self->numMediumLight++] = t;
		if (g_IsMapOutDoor) {
			radiusDef = g_MNTrafficCapsule;
			velDef = 150.0f;
		} else {
			radiusDef = g_MNTrafficCapsule * 0.5f;
			velDef = 100.0f;
		}
	} else {
		return;
	}
	self->tracks[t].radius = WorldInfo_GetFloat("info", "radius", radiusDef);
	self->tracks[t].speed = WorldInfo_GetFloat("info", "vel", velDef);
	self->tracks[t].speed = self->tracks[t].speed * 27.78f;
}

void CNodesTraffic_Load(CNodesTraffic *self, byte fill) {
	int ids[50], lists[5][50];
	char name[0x50];
	vec3_t pos, prev;
	float ang[2];
	byte flag;

	self->cellSize = g_IsMapOutDoor ? 25000.0f : 12500.0f;
	self->numTracks = 0;
	self->numNodes = 0;
	for (int i = 0; i < 50; i++) {
		Bprintf(name, sizeof(name), "DM_Track%d_*", i);
		DummySearch_Start(name, 1);
		if (DummySearch_Next(name, 0x32, &pos, ang, &flag)) {
			ids[self->numTracks++] = i;
		}
	}
	if (!self->numTracks) {
		return;
	}
	self->tracks = Mem_Alloc(self->numTracks * sizeof(cTrafficTrack));
	if (!self->tracks) {
		Con_Bluef("ERROR AI: IniNodesTraffic - Insuficiente memoria ListTrafficTrack.\n");
		CHECK(!"cNodesTraffic_IniNodesTraffic_new_ListTrafficTrack_Fail");
	}
	/* the number of nodes of each track: the dummies and the nodes fill puts between them */
	for (int t = 0; t < self->numTracks; t++) {
		int k = 0;

		self->tracks[t].numNodes = 0;
		prev.x = prev.y = prev.z = 3.4028235e38f;
		Bprintf(name, sizeof(name), "DM_Track%d_%d", ids[t], 0);
		DummySearch_Start(name, 1);
		while (DummySearch_Next(name, 0x32, &pos, ang, &flag)) {
			self->tracks[t].numNodes++;
			if (fill && self->tracks[t].numNodes > 1) {
				float dx = pos.x - prev.x, dy = pos.y - prev.y, dz = pos.z - prev.z;
				float len = (float)__builtin_sqrt((double)(dy * dy + dx * dx + dz * dz));
				self->tracks[t].numNodes += (int)floor((double)(len / g_MinDistNodeTrafficAI));
			}
			prev = pos;
			k++;
			Bprintf(name, sizeof(name), "DM_Track%d_%d", ids[t], k);
			DummySearch_Start(name, 1);
		}
		self->numNodes += self->tracks[t].numNodes;
		self->tracks[t].nodes = Mem_Alloc(self->tracks[t].numNodes * sizeof(vec3_t));
		if (!self->tracks[t].nodes) {
			Con_Bluef("ERROR AI: IniNodesTraffic - Insuficiente memoria ListNodes de track.\n");
			CHECK(!"cNodesTraffic_IniNodesTraffic_new_ListNodes_Fail");
		}
	}
	self->numHeavy = 0;
	self->numMedium = 0;
	self->numLight = 0;
	self->numHeavyLight = 0;
	self->numMediumLight = 0;
	for (int t = 0; t < self->numTracks; t++) {
		int id = ids[t], w = 0, k = 0;

		if (self->tracks[t].numNodes <= 0) {
			continue;
		}
		do {
			Bprintf(name, sizeof(name), "DM_Track%d_%d", id, k);
			DummySearch_Start(name, 1);
			if (!DummySearch_Next(name, 0x32, &pos, ang, &flag)) {
				Con_Bluef("ERROR AI: IniNodesTraffic - Dummy Tr\xe1"
					  "fico No Encontrado: %s.\n",
					name);
				CHECK(!"cNodesTraffic_IniNodesTraffic_Dummy_Traffic_Not_Found");
			}
			if (w == 0) {
				if (!WorldInfo_HasUserProp()) {
					Con_Bluef("ERROR AI: IniNodesTraffic - Properties Dummy Tr\xe1"
						  "fico No Encontrado: %s.\n",
						name);
					CHECK(!"cNodesTraffic_IniNodesTraffic_Properties_Dummy_Traffic_Not_Found");
				} else {
					Traffic_ReadInfo(self, t, id, lists);
				}
			} else if (fill) {
				/* the nodes on the line from the last one, at most MinDistNodeTrafficAI apart */
				const vec3_t *last = &self->tracks[t].nodes[w - 1];
				float dx = pos.x - last->x, dy = pos.y - last->y, dz = pos.z - last->z;
				vec3_t dir;
				float len = (float)__builtin_sqrt((double)(dy * dy + dx * dx + dz * dz));
				int n = (int)floor((double)(len / g_MinDistNodeTrafficAI));
				if (len == 0.0f) {
					dir.x = dx;
					dir.y = dy;
					dir.z = dz;
				} else {
					float inv = 1.0f / len;
					dir.x = dx * inv;
					dir.y = dy * inv;
					dir.z = dz * inv;
				}
				float step = len / (float)(n + 1);
				float sx = dir.x * step, sy = dir.y * step, sz = dir.z * step;
				if (n >= 1) {
					int i = w;
					w += n;
					for (; i < w; i++) {
						vec3_t *nd = &self->tracks[t].nodes[i];
						nd->x = nd[-1].x + sx;
						nd->y = nd[-1].y + sy;
						nd->z = nd[-1].z + sz;
					}
				}
			}
			self->tracks[t].nodes[w] = pos;
			w++;
			k++;
		} while (w < self->tracks[t].numNodes);
	}
	self->heavy = Traffic_SetList(self->heavy, lists[0], self->numHeavy,
		"ERROR AI: IniNodesTraffic - Insuficiente memoria ListTrafficTrackHeavy.\n",
		"cNodesTraffic_IniNodesTraffic_new_ListTrafficTrackHeavy_Fail", __LINE__);
	self->medium = Traffic_SetList(self->medium, lists[1], self->numMedium,
		"ERROR AI: IniNodesTraffic - Insuficiente memoria ListTrafficTrackMedium.\n",
		"cNodesTraffic_IniNodesTraffic_new_ListTrafficTrackMedium_Fail", __LINE__);
	self->light = Traffic_SetList(self->light, lists[2], self->numLight,
		"ERROR AI: IniNodesTraffic - Insuficiente memoria ListTrafficTrackLight.\n",
		"cNodesTraffic_IniNodesTraffic_new_ListTrafficTrackLight_Fail", __LINE__);
	self->heavyLight = Traffic_SetList(self->heavyLight, lists[3], self->numHeavyLight,
		"ERROR AI: IniNodesTraffic - Insuficiente memoria ListTrafficTrackHeavyLight.\n",
		"cNodesTraffic_IniNodesTraffic_new_ListTrafficTrackHeavyLight_Fail", __LINE__);
	self->mediumLight = Traffic_SetList(self->mediumLight, lists[4], self->numMediumLight,
		"ERROR AI: IniNodesTraffic - Insuficiente memoria ListTrafficTrackMediumLight.\n",
		"cNodesTraffic_IniNodesTraffic_new_ListTrafficTrackMediumLight_Fail", __LINE__);
	self->vftable->BuildGrid(self);
}

byte CNodesTraffic_RandomNode(CNodesTraffic *self, vec3_t *out, int type, const vec3_t *pos, float distMin,
	float distMax, int unused, const vec3_t *dir, float maxAngle, int *outTrack, int *outNode) {
	InfoTrafficNode list[500];
	int count;

	(void)unused;
	self->vftable->FindNodes(self, pos, type, list, 500, &count, 0.0f, distMin, distMax, 0, 0);
	if (!count) {
		return 0;
	}
	int i = crt_rand() % count;
	int up = crt_rand() % 2;
	const cTrafficTrack *tracks = self->tracks;
	for (int k = 0; k < count; k++) {
		int t = list[i].track, n = list[i].node;
		if (tracks[t].loop || n != tracks[t].numNodes - 1) {
			const vec3_t *p = &tracks[t].nodes[n];
			vec3_t d = { p->x - pos->x, p->y - pos->y, p->z - pos->z };
			if ((double)maxAngle > Vec3_Angle(dir, &d)) {
				*out = tracks[t].nodes[n];
				*outTrack = t;
				*outNode = (n + 1) % self->tracks[t].numNodes;
				return 1;
			}
		}
		if (up) {
			i = (i + 1) % count;
		} else if (i == 0) {
			i = count - 1;
		} else {
			i--;
		}
	}
	return 0;
}

byte CNodesTraffic_GetDirs(CNodesTraffic *self, vec3_t *prev, vec3_t *next, int track, int node) {
	vec3_t p, q;
	int t, n;

	if (!self->vftable->GetNode(self, &p, track, node)) {
		return 0;
	}
	if (!self->vftable->NextAway(self, &q, track, node, &t, &n)) {
		next->x = 0.0f;
		next->y = 0.0f;
		next->z = 0.0f;
	} else {
		Traffic_Normalize(next, q.x - p.x, q.y - p.y, q.z - p.z);
	}
	if (!self->vftable->PrevAway(self, &q, track, node, &t, &n)) {
		prev->x = 0.0f;
		prev->y = 0.0f;
		prev->z = 0.0f;
		return 1;
	}
	Traffic_Normalize(prev, q.x - p.x, q.y - p.y, q.z - p.z);
	return 1;
}

/* out at least 100 away from p in one axis */
static int Traffic_Away(const vec3_t *out, const vec3_t *p) {
	return !(100.0 > __builtin_fabs((double)(out->x - p->x))) || !(100.0 > __builtin_fabs((double)(out->y - p->y))) ||
		!(100.0 > __builtin_fabs((double)(out->z - p->z)));
}

byte CNodesTraffic_NextAway(CNodesTraffic *self, vec3_t *out, int track, int node, int *outTrack,
	int *outNode) {
	vec3_t p;

	if (node == self->tracks[track].numNodes - 1 && !self->tracks[track].loop) {
		return 0;
	}
	self->vftable->GetNode(self, &p, track, node);
	*out = p;
	*outTrack = track;
	*outNode = node;
	while (!Traffic_Away(out, &p)) {
		const cTrafficTrack *t = &self->tracks[*outTrack];
		int n = t->numNodes;
		if (*outNode == n - 1 && !t->loop) {
			return 0;
		}
		*outNode = (*outNode + 1) % n;
		*out = self->tracks[*outTrack].nodes[*outNode];
	}
	return 1;
}

byte CNodesTraffic_PrevAway(CNodesTraffic *self, vec3_t *out, int track, int node, int *outTrack,
	int *outNode) {
	vec3_t p;

	if (node == 0 && !self->tracks[track].loop) {
		return 0;
	}
	self->vftable->GetNode(self, &p, track, node);
	*out = p;
	*outTrack = track;
	*outNode = node;
	while (!Traffic_Away(out, &p)) {
		int n = *outNode;
		if (n == 0 && !self->tracks[*outTrack].loop) {
			return 0;
		}
		if (n < 1) {
			n = self->tracks[*outTrack].numNodes;
		}
		*outNode = n - 1;
		*out = self->tracks[*outTrack].nodes[*outNode];
	}
	return 1;
}

byte CNodesTraffic_GetNode(CNodesTraffic *self, vec3_t *out, int track, int node) {
	if (track > self->numTracks && node > self->tracks[track].numNodes) {
		return 0;
	}
	*out = self->tracks[track].nodes[node];
	return 1;
}

cTrafficTrackInfo *CNodesTraffic_GetInfo(CNodesTraffic *self, cTrafficTrackInfo *out, int track) {
	CHECK((track >= 0) && (track < self->numTracks));
	const cTrafficTrack *t = &self->tracks[track];
	out->numNodes = t->numNodes;
	out->type = t->type;
	out->speed = t->speed;
	out->radius = t->radius;
	out->loop = t->loop;
	out->id = t->id;
	return out;
}

void CNodesTraffic_Draw(CNodesTraffic *self) {
	if (g_TrafficState) {
		for (int i = 0; i < self->numTracks; i++) {
			self->vftable->DrawTrack(self, i);
		}
		if (g_TrafficState == 3) {
			cTrafficErrorNode *e = self->errors.head;
			self->errors.iter = e;
			while (e) {
				DebugUtils_DrawSphere(&e->pos, e->radius, 0xff00ff, 20, 20);
				e = self->errors.iter->next;
				self->errors.iter = e;
				if (e == self->errors.head) {
					break;
				}
			}
		}
		if (g_TrafficState == 4 && self->grid) {
			self->grid->vftable->Draw(self->grid, 0xffffff);
		}
	}
	if (g_ShowNumTrafficAI) {
		self->vftable->DrawTrack(self, g_ShowNumTrafficAI);
	}
}

void CNodesTraffic_DrawTrack(CNodesTraffic *self, int track) {
	uint32_t color;
	vec3_t p;

	if (track >= self->numTracks) {
		Con_Bluef("\nTRACK: %d INEXISTENTE\n", track);
		return;
	}
	switch (self->tracks[track].type) {
	case TRAFFIC_HEAVY:
		color = 0xff;
		break;
	case TRAFFIC_LIGHT:
		color = 0xff0000;
		break;
	case TRAFFIC_MEDIUM:
		color = 0xff00;
		break;
	case TRAFFIC_HEAVY_LIGHT:
		color = 0xff00ff;
		break;
	case TRAFFIC_MEDIUM_LIGHT:
		color = 0xffff00;
		break;
	default:
		color = track;
		break;
	}
	g_FontTerminal8->color = 0xffffff;
	g_FontTerminal8->align = 4;
	const cTrafficTrack *t = &self->tracks[track];
	for (int k = 0; k < t->numNodes - 1; k++) {
		if (g_TrafficState == 2) {
			const vec3_t *n = &t->nodes[k];
			p.x = n->x + 0.0f;
			p.y = n->y - 100.0f;
			p.z = n->z + 0.0f;
			CFont_Printf3D(g_FontTerminal8, &p, 3, "DM_Track%d_%d", track, k);
		}
		DebugUtils_DrawSphere(&self->tracks[track].nodes[k], 50.0f, color, 2, 2);
		const vec3_t *a = &self->tracks[track].nodes[k];
		DebugUtils_DrawLine(a, a + 1, color);
		t = &self->tracks[track];
	}
	t = &self->tracks[track];
	if (g_TrafficState == 2) {
		const vec3_t *n = &t->nodes[t->numNodes - 1];
		p.x = n->x + 0.0f;
		p.y = n->y - 100.0f;
		p.z = n->z + 0.0f;
		CFont_Printf3D(g_FontTerminal8, &p, 3, "DM_Track%d_%d", track, t->numNodes - 1);
	}
	t = &self->tracks[track];
	DebugUtils_DrawSphere(&t->nodes[t->numNodes - 1], 50.0f, color, 2, 2);
	t = &self->tracks[track];
	if (t->loop) {
		DebugUtils_DrawLine(&t->nodes[0], &t->nodes[t->numNodes - 1], color);
	}
}

void CNodesTraffic_BuildGrid(CNodesTraffic *self) {
	NodesWorld_GetBox(&self->min, &self->max);
	if (g_IsMapOutDoor) {
		char name[0x14];
		vec3_t pos;
		float ang[2];
		byte flag;

		DummySearch_Start("DM_MapTop", 0);
		if (!DummySearch_Next(name, 0x14, &pos, ang, &flag)) {
			Con_Bluef("ERROR AI: Dummy DM_MapTop no encontrado.\n");
			CHECK(!"CNodesWorld_IniNodesWorld_DM_MapTop_Not_Found");
		}
		self->max.y = pos.y + 5000.0f;
	}
	double nx = ceil((double)((self->max.x - self->min.x) / self->cellSize));
	double ny = ceil((double)((self->max.y - self->min.y) / self->cellSize));
	double nz = ceil((double)((self->max.z - self->min.z) / self->cellSize));
	if (self->grid) {
		self->grid->vftable->Delete(self->grid, 1);
	}
	cGridElements *grid = operator_new(sizeof(cGridElements));
	if (grid) {
		grid = cGridElements_ctor(grid, &self->max, &self->min, (int)nx, (int)ny, (int)nz);
	}
	self->grid = grid;
	if (!grid) {
		CHECK(!"GridNodes");
	}
	for (int t = 0; t < self->numTracks; t++) {
		for (int n = 0; n < self->tracks[t].numNodes; n++) {
			InfoTrafficNode info = { t, n };
			self->grid->vftable->Add(self->grid, &self->tracks[t].nodes[n], &info);
		}
	}
}

void CNodesTraffic_FindNodes(CNodesTraffic *self, const vec3_t *pos, int type, InfoTrafficNode *list,
	int max, int *count, float radius, float distMin, float distMax, byte soft, byte vis) {
	int c[3];
	vec3_t cs, dir;
	cTrafficCell *cell;

	CHECK(distMin <= distMax);
	*count = 0;
	self->grid->vftable->CellCoords(self->grid, pos, &c[0], &c[1], &c[2]);
	self->grid->vftable->GetCellSize(self->grid, &cs);
	Traffic_SearchDir(&dir, pos);
	/* the point distMin along the diagonal still in the cell of pos: the nodes of the cell first */
	if (c[0] == (int)(((pos->x + dir.x * distMin) - self->min.x) / cs.x) &&
		c[1] == (int)(((pos->y + dir.y * distMin) - self->min.y) / cs.y) &&
		c[2] == (int)(((pos->z + dir.z * distMin) - self->min.z) / cs.z)) {
		if (!self->grid->vftable->GetCellAt(self->grid, pos, &cell)) {
			return;
		}
		self->vftable->CollectCell(self, pos, type, cell, list, max, count, radius, distMin, distMax, vis,
			soft == 0);
	}
	if (*count < max) {
		self->vftable->SearchRings(self, pos, type, list, max, count, radius, distMin, distMax, soft, vis);
	}
}

void CNodesTraffic_CollectCell(CNodesTraffic *self, const vec3_t *pos, int type, cTrafficCell *cell,
	InfoTrafficNode *list, int max, int *count, float radius, float distMin, float distMax, byte vis, byte useMax) {
	g_TrafficSearchDist = g_DistGetPVisTrafficAI;
	if (!cell) {
		return;
	}
	cell->iter = cell->head;
	if (!cell->head) {
		return;
	}
	InfoTrafficNode *info = &cell->head->info;
	while (*count < max) {
		const cTrafficTrack *t = &self->tracks[info->track];
		if (type == t->type &&
			((t->density != TRAFFIC_DENSITY_NORMAL || t->numVehicles < g_MaxNumTrackNormalDensityAI) &&
				(t->density != TRAFFIC_DENSITY_LOW || t->numVehicles < g_MaxNumTrackLowDensityAI))) {
			vec3_t p = t->nodes[info->node];
			float dx = pos->x - p.x, dy = pos->y - p.y, dz = pos->z - p.z;
			float d2 = dy * dy + dx * dx + dz * dz;
			if (g_TrafficSearchDist * g_TrafficSearchDist > d2 && (!useMax || distMax * distMax > d2) &&
				((!t->loop && info->node == 0) || d2 > distMin * distMin) &&
				(!vis || CNodesWorld_TestPath(g_NodesWorld, pos, &p, radius, radius, 0, 0))) {
				list[*count] = *info;
				(*count)++;
			}
		}
		cell->iter = cell->iter->next;
		if (cell->iter == cell->head) {
			return;
		}
		info = &cell->iter->info;
	}
}

/* one cell of a ring: its nodes collected; 1 when the search has enough */
static int Rings_Cell(CNodesTraffic *self, const int *x, const int *y, const int *z, const vec3_t *pos, int type,
	InfoTrafficNode *list, int max, int *count, float radius, float distMin, float distMax, byte soft, byte vis,
	float e) {
	cTrafficCell *cell;

	self->grid->vftable->GetCell(self->grid, x, y, z, &cell);
	self->vftable->CollectCell(self, pos, type, cell, list, max, count, radius, distMin, distMax, vis, soft == 0);
	return *count >= max || (*count > 0 && soft && e >= distMax);
}

/* a side of the ring box moved out one cell: 0 the side stops, 1 it grows, 2 the search ends */
static int Rings_Grow(int inGrid, float e, float distMax, byte soft, int count) {
	if (!inGrid || !(g_TrafficSearchDist > e) || (!soft && !(distMax > e))) {
		return 0;
	}
	if (count > 0 && soft && e >= distMax) {
		return 2;
	}
	return 1;
}

void CNodesTraffic_SearchRings(CNodesTraffic *self, const vec3_t *pos, int type, InfoTrafficNode *list,
	int max, int *count, float radius, float distMin, float distMax, byte soft, byte vis) {
	int c[3], n[3], x, y, z;
	vec3_t cs, dir;
	byte stopXhi = 0, stopXlo = 0, stopYhi = 0, stopYlo = 0, stopZhi = 0, stopZlo = 0;

	g_TrafficSearchDist = g_DistGetPVisTrafficAI;
	CHECK(distMin <= distMax);
	self->grid->vftable->CellCoords(self->grid, pos, &c[0], &c[1], &c[2]);
	self->grid->vftable->GetDivs(self->grid, &n[0], &n[1], &n[2]);
	self->grid->vftable->GetCellSize(self->grid, &cs);
	Traffic_SearchDir(&dir, pos);
	/* the start box: from the cell of pos to the cell of the point distMin along the diagonal */
	int dx = (int)(((pos->x + dir.x * distMin) - self->min.x) / cs.x) - c[0];
	if (!dx) {
		dx = 1;
	}
	int dy = (int)(((pos->y + dir.y * distMin) - self->min.y) / cs.y) - c[1];
	if (!dy) {
		dy = 1;
	}
	int dz = (int)(((pos->z + dir.z * distMin) - self->min.z) / cs.z) - c[2];
	if (!dz) {
		dz = 1;
	}
	int hiX = c[0] - 1 + dx;
	if (hiX >= n[0]) {
		hiX = n[0] - 1;
	}
	int loX = c[0] - dx + 1;
	if (loX < 0) {
		loX = 0;
	}
	int hiY = c[1] - 1 + dy;
	if (hiY >= n[1]) {
		hiY = n[1] - 1;
	}
	int loY = c[1] - dy + 1;
	if (loY < 0) {
		loY = 0;
	}
	int hiZ = c[2] - 1 + dz;
	if (hiZ >= n[2]) {
		hiZ = n[2] - 1;
	}
	int loZ = c[2] - dz + 1;
	if (loZ < 0) {
		loZ = 0;
	}
#define RINGS_STEP(stop, cond, e, op) \
	if (!(stop)) { \
		int g = Rings_Grow((cond), (e), distMax, soft, *count); \
		if (g == 2) { \
			return; \
		} \
		if (g) { \
			op; \
		} else { \
			(stop) = 1; \
		} \
	}
#define RINGS_CELL(px, py, pz, e) \
	if (Rings_Cell(self, (px), (py), (pz), pos, type, list, max, count, radius, distMin, distMax, soft, vis, (e))) { \
		return; \
	}
	for (int r = 0;; r++) {
		float fr = (float)r;
		float ex = fr * cs.x, ey = fr * cs.y, ez = fr * cs.z;

		RINGS_STEP(stopXhi, hiX + 1 < n[0], ex, hiX++)
		RINGS_STEP(stopXlo, loX - 1 >= 0, ex, loX--)
		RINGS_STEP(stopYhi, hiY + 1 < n[1], ey, hiY++)
		RINGS_STEP(stopYlo, loY - 1 >= 0, ey, loY--)
		RINGS_STEP(stopZhi, hiZ + 1 < n[2], ez, hiZ++)
		RINGS_STEP(stopZlo, loZ - 1 >= 0, ez, loZ--)
		if (stopXhi && stopXlo && stopYhi && stopYlo && stopZhi && stopZlo) {
			return;
		}
		/* the new faces of the box: x over all y z, y without the x faces, z without the x and y faces */
		if (!stopXhi) {
			for (y = loY; y <= hiY; y++) {
				for (z = loZ; z <= hiZ; z++) {
					RINGS_CELL(&hiX, &y, &z, ex)
				}
			}
		}
		if (!stopXlo) {
			for (y = loY; y <= hiY; y++) {
				for (z = loZ; z <= hiZ; z++) {
					RINGS_CELL(&loX, &y, &z, ex)
				}
			}
		}
		int xs = loX + !stopXlo, xe = hiX - !stopXhi;
		if (!stopYhi) {
			for (x = xs; x <= xe; x++) {
				for (z = loZ; z <= hiZ; z++) {
					RINGS_CELL(&x, &hiY, &z, ey)
				}
			}
		}
		if (!stopYlo) {
			for (x = xs; x <= xe; x++) {
				for (z = loZ; z <= hiZ; z++) {
					RINGS_CELL(&x, &loY, &z, ey)
				}
			}
		}
		int ys = loY + !stopYlo, ye = hiY - !stopYhi;
		if (!stopZhi) {
			for (x = xs; x <= xe; x++) {
				for (y = ys; y <= ye; y++) {
					RINGS_CELL(&x, &y, &hiZ, ez)
				}
			}
		}
		if (!stopZlo) {
			for (x = xs; x <= xe; x++) {
				for (y = ys; y <= ye; y++) {
					RINGS_CELL(&x, &y, &loZ, ez)
				}
			}
		}
	}
#undef RINGS_STEP
#undef RINGS_CELL
}

/* a node 100 away after or before the node, the side by rand(); 1 if it is the node after */
static byte Traffic_AwayNode(CNodesTraffic *self, vec3_t *p, int track, int node, int *t, int *n) {
	if (crt_rand() % 2 == 0) {
		if (self->vftable->NextAway(self, p, track, node, t, n)) {
			return 1;
		}
		self->vftable->PrevAway(self, p, track, node, t, n);
		return 0;
	}
	if (self->vftable->PrevAway(self, p, track, node, t, n)) {
		return 0;
	}
	self->vftable->NextAway(self, p, track, node, t, n);
	return 1;
}

void CNodesTraffic_RandomPointNear(CNodesTraffic *self, int track, int node, float maxDist, vec3_t *out,
	int *outTrack, int *outNode) {
	vec3_t p = { 0.0f, 0.0f, 0.0f }, dir;
	int t = 0, n = 0;

	byte next = Traffic_AwayNode(self, &p, track, node, &t, &n);
	const vec3_t *nd = &self->tracks[track].nodes[node];
	float len = Traffic_Normalize(&dir, p.x - nd->x, p.y - nd->y, p.z - nd->z);
	float f = FxElem_Rand((double)Float_MinSS(len, maxDist), 0.0);
	nd = &self->tracks[track].nodes[node];
	out->x = nd->x + dir.x * f;
	out->y = nd->y + dir.y * f;
	out->z = nd->z + dir.z * f;
	*outTrack = t;
	if (track == t) {
		*outNode = next ? n : node;
	}
}

void CNodesTraffic_RandomPointAhead(CNodesTraffic *self, int track, int node, const vec3_t *pos,
	float distMin, float distMax, int unused, const vec3_t *dir, float maxAngle, vec3_t *out, int *outTrack,
	int *outNode) {
	vec3_t p = { 0.0f, 0.0f, 0.0f }, u, q, d;
	int t = 0, n = 0;

	(void)unused;
	byte next = Traffic_AwayNode(self, &p, track, node, &t, &n);
	const vec3_t *nd = &self->tracks[track].nodes[node];
	float len = Traffic_Normalize(&u, p.x - nd->x, p.y - nd->y, p.z - nd->z);
	float f = FxElem_Rand((double)len, 0.0);
	nd = &self->tracks[track].nodes[node];
	q.x = nd->x + u.x * f;
	q.y = nd->y + u.y * f;
	q.z = nd->z + u.z * f;
	d.x = q.x - pos->x;
	d.y = q.y - pos->y;
	d.z = q.z - pos->z;
	float dist = (float)__builtin_sqrt((double)(d.y * d.y + d.x * d.x + d.z * d.z));
	if (dist > distMin && distMax > dist && (double)maxAngle > Vec3_Angle(dir, &d)) {
		*out = q;
		*outTrack = t;
		if (track == t) {
			*outNode = next ? n : node;
		}
		return;
	}
	*outTrack = track;
	const cTrafficTrack *tr = &self->tracks[track];
	if (node < tr->numNodes - 1 || tr->loop) {
		*outNode = (node + 1) % tr->numNodes;
	} else {
		*outNode = node;
	}
}

/* ---- the analysis of the tracks (writes <level>/AI/AnalisisTraffic.txt) ---- */

#define NT_POINTFMT "TRACK: %d NODO: %d - TRACK: %d NODO: %d -> Point: %.3f %.3f %.3f\n"
#define NT_OVERLAPFMT "SEGMENTOS TRACK SUPERPUESTOS -> SEGMENTO 1: TRACK: %d NODO: %d - TRACK: %d NODO: %d -- " \
	"SEGMENTO 2: TRACK: %d NODO: %d - TRACK: %d NODO: %d\n"

static void NT_Write(void *f, const char *text) {
	crt_fwrite(text, 1, strlen(text), f);
}

/* the point of a track in the level into the errors and the file (in meters, y and z swapped) */
static void NT_Error(cTrafficErrorList *errors, vec3_t *hit, float radius, void *f, int id, int a, int b) {
	char buf[0xc8];
	cTrafficErrorNode *n = DList_NewNode(errors, sizeof(*n));

	n->pos = *hit;
	n->radius = radius;
	DList_LinkFirst(errors, n);
	float z = hit->y / 100.0f;
	float y = hit->z / 100.0f;
	float x = hit->x / 100.0f;
	hit->z = z;
	hit->x = x;
	hit->y = y;
	Bprintf(buf, sizeof(buf), NT_POINTFMT, id, a, id, b, (double)x, (double)y, (double)z);
	NT_Write(f, buf);
}

/* the node b of the track lies on the segment back to a: both directions from b the same */
static int NT_Overlap(const vec3_t *a, const vec3_t *b, const vec3_t *c) {
	vec3_t u, w, r;

	NW_Normalize(&u, a->x - b->x, a->y - b->y, a->z - b->z);
	NW_Normalize(&w, c->x - b->x, c->y - b->y, c->z - b->z);
	NW_Normalize(&r, u.x - w.x, u.y - w.y, u.z - w.z);
	return (double)0.1f > __builtin_fabs((double)(r.x - 0.0f)) && (double)0.1f > __builtin_fabs((double)(r.y - 0.0f)) &&
		(double)0.1f > __builtin_fabs((double)(r.z - 0.0f));
}

void CNodesTraffic_Analyze(CNodesTraffic *self) {
	char path[0x100], buf[0xc8];
	vec3_t bmin, bmax, hit;
	byte bad = 0;
	int numBad = 0;

	self->vftable->Load(self, 0);
	size_t n = strnlen(g_pApp->levelPath, sizeof(path) - 0x18);
	memcpy(path, g_pApp->levelPath, n);
	memcpy(path + n, "/AI/AnalisisTraffic.txt", sizeof("/AI/AnalisisTraffic.txt"));
	void *f = crt_fopen(path, "wt");
	if (!f) {
		Con_Bluef("ERROR: Fichero AnalisisTraffic no abierto.\n");
		CHECK(!"CNodesWorld_AnalizeTraffic_File_AnalisisTraffic_Not_Open");
	}
	cTrafficErrorList *errors = &self->errors;
	TrafficErrors_Clear(errors);
	NT_Write(f, "SEGMENTOS TRAFICO COLISIONANDO CON ESCENARIO\n\n");
	g_NodesWorld->sup.vftable->GetBox(g_NodesWorld, &bmin, &bmax);
	for (int i = 0; i < self->numTracks; i++) {
		cTrafficTrack *tr = &self->tracks[i];
		for (int j = 0; j < tr->numNodes - 1; j++) {
			float radius = tr->radius;
			if (g_IsMapOutDoor && tr->nodes[j].y + tr->radius >= bmax.y) {
				hit = tr->nodes[j];
				NT_Error(errors, &hit, radius, f, tr->id, j, j + 1);
				bad = 1;
				numBad++;
			}
			if (Engine_TestSphere(&tr->nodes[j], &tr->nodes[j + 1], tr->radius, &hit, NULL)) {
				NT_Error(errors, &hit, radius, f, tr->id, j, j + 1);
				bad = 1;
				numBad++;
			}
			if (j > 0 && j < tr->numNodes - 1 && NT_Overlap(&tr->nodes[j - 1], &tr->nodes[j], &tr->nodes[j + 1])) {
				Bprintf(buf, sizeof(buf), NT_OVERLAPFMT, tr->id, j - 1, tr->id, j, tr->id, j, tr->id, j + 1);
				NT_Write(f, buf);
			}
		}
		int last = tr->numNodes;
		float radius = tr->radius;
		if (g_IsMapOutDoor && tr->nodes[last - 1].y + tr->radius >= bmax.y) {
			hit = tr->nodes[last - 1];
			NT_Error(errors, &hit, radius, f, tr->id, last - 1, last);
			bad = 1;
			numBad++;
		}
		if (tr->loop) {
			if (Engine_TestSphere(&tr->nodes[0], &tr->nodes[last - 1], tr->radius, &hit, NULL)) {
				NT_Error(errors, &hit, tr->radius, f, tr->id, 0, tr->numNodes - 1);
				bad = 1;
				numBad++;
			}
			if (NT_Overlap(&tr->nodes[tr->numNodes - 1], &tr->nodes[0], &tr->nodes[1])) {
				Bprintf(buf, sizeof(buf), NT_OVERLAPFMT, tr->id, last - 2, tr->id, last - 1, tr->id, last - 1, tr->id,
					last);
				NT_Write(f, buf);
			}
		}
	}
	if (bad) {
		NT_Write(f, "\n\nTRAFICO ERRONEO\n");
		Con_Bluef("%s", "\n\nTRAFICO ERRONEO\n");
		Bprintf(buf, sizeof(buf), "NUM. PUNTOS TRAFICO ERRONEOS: %d\n", numBad);
		NT_Write(f, buf);
		Con_Bluef("%s", buf);
	} else {
		NT_Write(f, "\n\nTRAFICO CORRECTO\n");
		Con_Printf("%s", "\n\nTRAFICO CORRECTO\n");
	}
	if (crt_fclose(f)) {
		Con_Bluef("ERROR: Fichero AnalisisTraffic no cerrado.\n");
		CHECK(!"CNodesWorld_AnalizeTraffic_File_AnalisisTraffic_Not_Closed");
	}
	self->vftable->Load(self, 1);
}

/* the slot BuildGrid of a class that does nothing there */
void CNodesTraffic_BuildGridNop(CNodesTraffic *self) {
	(void)self;
}

/* the vtable of CNodesTraffic */
const CNodesTrafficVtbl CNodesTraffic_vftable = {
	.Delete = CNodesTraffic_Delete,
	.AddVehicle = CNodesTraffic_AddVehicle,
	.RemoveVehicle = CNodesTraffic_RemoveVehicle,
	.NumTracks = CNodesTraffic_NumTracks,
	.NumByType = CNodesTraffic_NumByType,
	.NumNodes = CNodesTraffic_NumNodes,
	.GetRadius = CNodesTraffic_GetRadius,
	.Init = CNodesTraffic_Init,
	.Load = CNodesTraffic_Load,
	.RandomNode = CNodesTraffic_RandomNode,
	.GetDirs = CNodesTraffic_GetDirs,
	.NextAway = CNodesTraffic_NextAway,
	.PrevAway = CNodesTraffic_PrevAway,
	.GetNode = CNodesTraffic_GetNode,
	.GetInfo = CNodesTraffic_GetInfo,
	.Draw = CNodesTraffic_Draw,
	.Analyze = CNodesTraffic_Analyze,
	.DrawTrack = CNodesTraffic_DrawTrack,
	.BuildGrid = CNodesTraffic_BuildGrid,
	.FindNodes = CNodesTraffic_FindNodes,
	.CollectCell = CNodesTraffic_CollectCell,
	.SearchRings = CNodesTraffic_SearchRings,
	.RandomPointNear = CNodesTraffic_RandomPointNear,
	.RandomPointAhead = CNodesTraffic_RandomPointAhead,
};

/* the vtable of cGridElements_InfoTrafficNode */
const cGridElementsVtbl cGridElements_InfoTrafficNode_vftable = {
	.Delete = cGridElements_Delete,
	.Add = cGridElements_Add,
	.GetCellAt = cGridElements_GetCellAt,
	.GetCell = cGridElements_GetCell,
	.Draw = cGridElements_Draw,
	.CellCoords = cGridElements_CellCoords,
	.GetDivs = cGridElements_GetDivs,
	.GetCellSize = cGridElements_GetCellSize,
	.IsInside = cGridElements_IsInside,
	.Free = cGridElements_Free,
	.CellIndex = cGridElements_CellIndex,
};
