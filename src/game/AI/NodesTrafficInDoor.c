/*
 * The traffic graph of the indoor levels, original source AI/NodesTrafficInDoor.cpp (include/scrap/nodestraffic.h).
 * No grid: the nodes of the tracks are kept by their sectors, and each frame the waiting traffic vehicles are put on
 * the tracks of the sectors that came into view.
 */
#include "scrap/console.h"
#include "scrap/nodestraffic.h"
#include "scrap/scrapmain.h"
#include "scrap/scrapmodel.h"
#include "scrap/scrapworld.h"
#include "scrap/vehicleentity.h"

/* the step through the tracks of a sector from a random start, up or down */
static int Sector_NextTrack(int j, int up, int numT) {
	if (up) {
		return (j + 1) % numT;
	}
	if (j == 0) {
		j = numT;
	}
	return j - 1;
}

CNodesTraffic *__thiscall CNodesTrafficInDoor_ctor(void *mem) {
	CNodesTrafficInDoor *self = mem;

	CNodesTraffic_ctor(mem);
	self->base.vftable = CNODESTRAFFICINDOOR_VFTABLE;
	self->heavy.list = NULL;
	self->medium.list = NULL;
	self->light.list = NULL;
	self->heavyLight.list = NULL;
	self->mediumLight.list = NULL;
	self->heavy.num = 0;
	self->medium.num = 0;
	self->light.num = 0;
	self->heavyLight.num = 0;
	self->mediumLight.num = 0;
	self->numHeavyAI = 0;
	self->numMediumAI = 0;
	self->numLightAI = 0;
	self->seenHeavy.num = 0;
	self->seenHeavy.numNodes = 0;
	self->seenMedium.num = 0;
	self->seenMedium.numNodes = 0;
	self->seenLight.num = 0;
	self->seenLight.numNodes = 0;
	self->seenHeavyLight.num = 0;
	self->seenHeavyLight.numNodes = 0;
	self->seenMediumLight.num = 0;
	self->seenMediumLight.numNodes = 0;
	Engine_SetCallback((void (*)(void))(void *)TrafficInDoor_Frame, 2);
	return mem;
}

static void Sectors_Free(cTrafficSectorList *l) {
	if (!l->list) {
		return;
	}
	for (int i = 0; i < l->num; i++) {
		Mem_Free(l->list[i].nodes);
		Mem_Free(l->list[i].tracks);
	}
	Mem_Free(l->list);
	l->list = NULL;
}

void __fastcall CNodesTrafficInDoor_dtor(CNodesTrafficInDoor *self) {
	self->base.vftable = CNODESTRAFFICINDOOR_VFTABLE;
	Sectors_Free(&self->heavy);
	Sectors_Free(&self->medium);
	Sectors_Free(&self->light);
	Sectors_Free(&self->heavyLight);
	Sectors_Free(&self->mediumLight);
	Engine_SetCallback(NULL, 2);
	CNodesTraffic_dtor(&self->base);
}

CNodesTrafficInDoor *__thiscall CNodesTrafficInDoor_Delete(CNodesTrafficInDoor *self, byte flags) {
	CNodesTrafficInDoor_dtor(self);
	if (flags & 1) {
		operator_delete_sized(self, sizeof(*self));
	}
	return self;
}

byte TrafficInDoor_Frame(void) {
	CNodesTrafficInDoor_UpdateVehicles((CNodesTrafficInDoor *)g_NodesTrafficInDoor);
	return 1;
}

void __thiscall CNodesTrafficInDoor_UpdateVehicles(CNodesTrafficInDoor *self) {
	int numSeen = 0, numNodes = 0;

	CNodesTrafficInDoor_Respawn(self, self->heavyAI, self->numHeavyAI, TRAFFIC_HEAVY, self->heavy.list,
		self->heavy.num, self->seenHeavy.index, &self->seenHeavy.num, &self->seenHeavy.numNodes, 1,
		TRAFFIC_HEAVY_LIGHT, self->heavyLight.list, self->heavyLight.num, self->seenHeavyLight.index,
		&self->seenHeavyLight.num, &self->seenHeavyLight.numNodes, 1, 5, NULL, 0, NULL, &numSeen, &numNodes, 0);
	CNodesTrafficInDoor_Respawn(self, self->mediumAI, self->numMediumAI, TRAFFIC_MEDIUM, self->medium.list,
		self->medium.num, self->seenMedium.index, &self->seenMedium.num, &self->seenMedium.numNodes, 1,
		TRAFFIC_MEDIUM_LIGHT, self->mediumLight.list, self->mediumLight.num, self->seenMediumLight.index,
		&self->seenMediumLight.num, &self->seenMediumLight.numNodes, 1, 5, NULL, 0, NULL, &numSeen, &numNodes, 0);
	CNodesTrafficInDoor_Respawn(self, self->lightAI, self->numLightAI, TRAFFIC_LIGHT, self->light.list,
		self->light.num, self->seenLight.index, &self->seenLight.num, &self->seenLight.numNodes, 1,
		TRAFFIC_HEAVY_LIGHT, self->heavyLight.list, self->heavyLight.num, self->seenHeavyLight.index,
		&self->seenHeavyLight.num, &self->seenHeavyLight.numNodes, 0, TRAFFIC_MEDIUM_LIGHT, self->mediumLight.list,
		self->mediumLight.num, self->seenMediumLight.index, &self->seenMediumLight.num,
		&self->seenMediumLight.numNodes, 0);
}

/* the sectors that came into view since the last update */
static void Sectors_UpdateSeen(cTrafficSector *sectors, int num, int *seen, int *numSeen, int *nodes) {
	*numSeen = 0;
	*nodes = 0;
	for (int i = 0; i < num; i++) {
		byte vis = Sector_Visible(sectors[i].sector);
		if (vis && !sectors[i].visible) {
			seen[(*numSeen)++] = i;
			*nodes += sectors[i].numNodes;
		}
		sectors[i].visible = vis;
	}
}

void __thiscall CNodesTrafficInDoor_Respawn(CNodesTrafficInDoor *self, cAIVehicle **ai, int numAI, int typeA,
	cTrafficSector *sectorsA, int numA, int *seenA, int *numSeenA, int *nodesA, byte updateA, int typeB,
	cTrafficSector *sectorsB, int numB, int *seenB, int *numSeenB, int *nodesB, byte updateB, int typeC,
	cTrafficSector *sectorsC, int numC, int *seenC, int *numSeenC, int *nodesC, byte updateC) {
	int track = 0;

	if (updateA) {
		Sectors_UpdateSeen(sectorsA, numA, seenA, numSeenA, nodesA);
	}
	if (updateB) {
		Sectors_UpdateSeen(sectorsB, numB, seenB, numSeenB, nodesB);
	}
	if (updateC) {
		Sectors_UpdateSeen(sectorsC, numC, seenC, numSeenC, nodesC);
	}
	if (*numSeenA <= 0 && *numSeenB <= 0 && *numSeenC <= 0) {
		return;
	}
	int total = *nodesC + *nodesB + *nodesA;
	for (int v = 0; v < numAI; v++) {
		int r = crt_rand() % total, type, n;
		cTrafficSector *sectors;
		const int *seen;

		if (r < *nodesA) {
			type = typeA;
			sectors = sectorsA;
			seen = seenA;
			n = *numSeenA;
		} else if (r < *nodesB + *nodesA) {
			type = typeB;
			sectors = sectorsB;
			seen = seenB;
			n = *numSeenB;
		} else {
			type = typeC;
			sectors = sectorsC;
			seen = seenC;
			n = *numSeenC;
		}
		if (cAIVehicle_GetState(ai[v]) != 9 || cAIVehicle_GetInternState(ai[v])) {
			continue;
		}
		const cTrafficSector *s = &sectors[seen[crt_rand() % n]];
		int j = crt_rand() % s->numTracks;
		int up = crt_rand() % 2;
		int numT = s->numTracks;
		if (numT > 0) {
			const int *tracks = s->tracks;
			const cTrafficTrack *tr = self->base.tracks;
			/* the first track with room for a vehicle, else the last one looked at */
			for (int c = 0; c < numT; c++) {
				track = tracks[j];
				const cTrafficTrack *t = &tr[track];
				if (t->density == TRAFFIC_DENSITY_NORMAL) {
					if (t->numVehicles < g_MaxNumTrackNormalDensityAI) {
						break;
					}
				} else if (t->density != TRAFFIC_DENSITY_LOW || t->numVehicles < g_MaxNumTrackLowDensityAI) {
					break;
				}
				j = Sector_NextTrack(j, up, numT);
			}
		}
		int node = crt_rand() % self->base.tracks[track].numNodes;
		vec3_t p;
		int oT, oN;
		self->base.vftable->RandomPointNear(&self->base, track, node, g_MaxDistAroundReposAI, &p, &oT, &oN);
		cAIVehicle_PlaceOnTrack(ai[v], type, &p, oT, oN);
	}
}

int __thiscall CNodesTrafficInDoor_HiddenNode(CNodesTrafficInDoor *self, int track) {
	int last = 0;

	for (int n = 0; n < self->base.tracks[track].numNodes; n++) {
		if (CNodesWorld_PointSeen(g_NodesWorld, &self->base.tracks[track].nodes[n])) {
			break;
		}
		last = n;
	}
	return last < self->base.tracks[track].numNodes - 1 ? last : 0;
}

void __stdcall TrafficSectors_Add(cTrafficSector *list, int *num, int sector) {
	for (int i = 0; i < *num; i++) {
		if (list[i].sector == sector) {
			list[i].numNodes++;
			return;
		}
	}
	list[*num].sector = sector;
	list[*num].numNodes = 1;
	(*num)++;
}

/* the sectors of a type made from the counts of the first pass */
static void Sectors_Alloc(cTrafficSectorList *l, const cTrafficSector *count, const char *check) {
	for (int i = 0; i < l->num; i++) {
		l->list[i].numNodes = 0;
		l->list[i].numTracks = 0;
		l->list[i].sector = count[i].sector;
		l->list[i].visible = 0;
		l->list[i].nodes = Mem_Alloc(count[i].numNodes << 3);
		if (!l->list[i].nodes) {
			Con_Bluef("ERROR AI: IniInfoSectorNodes - Insuficiente memoria ListInfSector.\n");
			CheckFailed(check, __FILE__, __TIMESTAMP__, __LINE__);
		}
	}
}

static cTrafficSector *Sectors_New(int num, const char *check) {
	cTrafficSector *list = Mem_Alloc(num * sizeof(cTrafficSector));
	if (!list) {
		Con_Bluef("ERROR AI: IniInfoSectorNodes - Insuficiente memoria ListInfSector.\n");
		CheckFailed(check, __FILE__, __TIMESTAMP__, __LINE__);
	}
	return list;
}

static cTrafficSector *Sectors_NewCount(const char *check) {
	cTrafficSector *list = Mem_Alloc(20 * sizeof(cTrafficSector));
	if (!list) {
		Con_Bluef("ERROR AI: IniInfoSectorNodes - Insuficiente memoria listInfSectors.\n");
		CheckFailed(check, __FILE__, __TIMESTAMP__, __LINE__);
	}
	return list;
}

void __fastcall CNodesTrafficInDoor_InitSectors(CNodesTrafficInDoor *self) {
	CNodesTraffic *b = &self->base;
	int ns = 0, np = 0;

	int *secs = Mem_Alloc(b->numNodes << 2);
	if (!secs) {
		Con_Bluef("ERROR AI: IniInfoSectorNodes - Insuficiente memoria listSectors.\n");
		CHECK(!"new_listSectors_Fail");
	}
	self->heavy.num = 0;
	cTrafficSector *cHeavy = Sectors_NewCount("new_listInfSectorsHeavy_Fail");
	self->medium.num = 0;
	cTrafficSector *cMedium = Sectors_NewCount("new_listInfSectorsMedium_Fail");
	self->light.num = 0;
	cTrafficSector *cLight = Sectors_NewCount("new_listInfSectorsLight_Fail");
	self->heavyLight.num = 0;
	cTrafficSector *cHeavyLight = Sectors_NewCount("new_listInfSectorsHeavyLight_Fail");
	self->mediumLight.num = 0;
	cTrafficSector *cMediumLight = Sectors_NewCount("new_listInfSectorsMediumLight_Fail");
	InfoTrafficNode *pairs = Mem_Alloc(b->numNodes << 3);
	if (!pairs) {
		Con_Bluef("ERROR AI: IniInfoSectorNodes - Insuficiente memoria listPointerNodes.\n");
		CHECK(!"new_listPointerNodes_Fail");
	}
	/* the sector of each node, counted by the type of its track */
	for (int t = 0; t < b->numTracks; t++) {
		for (int n = 0; n < b->tracks[t].numNodes; n++) {
			int sector = Sector_At(&b->tracks[t].nodes[n]);
			CHECK(sector >= 0);
			secs[ns++] = sector;
			pairs[np].track = t;
			pairs[np].node = n;
			np++;
			switch (b->tracks[t].type) {
			case TRAFFIC_HEAVY:
				TrafficSectors_Add(cHeavy, &self->heavy.num, sector);
				break;
			case TRAFFIC_LIGHT:
				TrafficSectors_Add(cLight, &self->light.num, sector);
				break;
			case TRAFFIC_MEDIUM:
				TrafficSectors_Add(cMedium, &self->medium.num, sector);
				break;
			case TRAFFIC_HEAVY_LIGHT:
				TrafficSectors_Add(cHeavyLight, &self->heavyLight.num, sector);
				break;
			case TRAFFIC_MEDIUM_LIGHT:
				TrafficSectors_Add(cMediumLight, &self->mediumLight.num, sector);
				break;
			}
		}
	}
	self->heavy.list = Sectors_New(self->heavy.num, "new_ListInfSectorHeavy_Fail");
	self->medium.list = Sectors_New(self->medium.num, "new_ListInfSectorMedium_Fail");
	self->light.list = Sectors_New(self->light.num, "new_ListInfSectorLight_Fail");
	self->heavyLight.list = Sectors_New(self->heavyLight.num, "new_ListInfSectorHeavyLight_Fail");
	self->mediumLight.list = Sectors_New(self->mediumLight.num, "new_ListInfSectorMediumLight_Fail");
	Sectors_Alloc(&self->heavy, cHeavy, "new_ListInfSectorHeavy_Fail");
	Sectors_Alloc(&self->medium, cMedium, "new_ListInfSectorMedium_Fail");
	Sectors_Alloc(&self->light, cLight, "new_ListInfSectorLight_Fail");
	Sectors_Alloc(&self->heavyLight, cHeavyLight, "new_ListInfSectorHeavyLight_Fail");
	Sectors_Alloc(&self->mediumLight, cMediumLight, "new_ListInfSectorMediumLight_Fail");
	/* the nodes into the sectors; a track of another type keeps the list of the node before */
	cTrafficSector *list = NULL;
	int num = 0;
	for (int i = 0; i < b->numNodes; i++) {
		switch (b->tracks[pairs[i].track].type) {
		case TRAFFIC_HEAVY:
			list = self->heavy.list;
			num = self->heavy.num;
			break;
		case TRAFFIC_LIGHT:
			list = self->light.list;
			num = self->light.num;
			break;
		case TRAFFIC_MEDIUM:
			list = self->medium.list;
			num = self->medium.num;
			break;
		case TRAFFIC_HEAVY_LIGHT:
			list = self->heavyLight.list;
			num = self->heavyLight.num;
			break;
		case TRAFFIC_MEDIUM_LIGHT:
			list = self->mediumLight.list;
			num = self->mediumLight.num;
			break;
		}
		for (int k = 0; k < num; k++) {
			if (secs[i] == list[k].sector) {
				cTrafficSector *s = &list[k];
				s->nodes[s->numNodes] = pairs[i];
				s->numNodes++;
				break;
			}
		}
	}
	TrafficSectors_SetTracks(self->heavy.list, self->heavy.num);
	TrafficSectors_SetTracks(self->medium.list, self->medium.num);
	TrafficSectors_SetTracks(self->light.list, self->light.num);
	TrafficSectors_SetTracks(self->heavyLight.list, self->heavyLight.num);
	TrafficSectors_SetTracks(self->mediumLight.list, self->mediumLight.num);
	Mem_Free(secs);
	Mem_Free(cHeavy);
	Mem_Free(cMedium);
	Mem_Free(cLight);
	Mem_Free(cHeavyLight);
	Mem_Free(cMediumLight);
	Mem_Free(pairs);
}

void __thiscall CNodesTrafficInDoor_Init(CNodesTrafficInDoor *self) {
	self->base.vftable->Load(&self->base, 1);
	if (self->base.numTracks > 0) {
		CNodesTrafficInDoor_InitSectors(self);
	}
}

/* vtable slot 0x48: no grid indoors */
void __thiscall CNodesTrafficInDoor_BuildGrid(CNodesTrafficInDoor *self) {
	(void)self;
}

void __stdcall TrafficSectors_SetTracks(cTrafficSector *list, int num) {
	int tracks[100];

	for (int i = 0; i < num; i++) {
		cTrafficSector *s = &list[i];
		int n = 0;
		for (int k = 0; k < s->numNodes; k++) {
			int j = 0;
			while (j < n && s->nodes[k].track != tracks[j]) {
				j++;
			}
			if (j == n) {
				tracks[n++] = s->nodes[k].track;
			}
		}
		s->tracks = Mem_Alloc(n * 4);
		if (!s->tracks) {
			Con_Bluef("ERROR AI: IniInfoSectorNodes - Insuficiente memoria ListTracks.\n");
			CHECK(!"new_ListTracks_Fail");
		}
		s->numTracks = n;
		for (int k = 0; k < n; k++) {
			s->tracks[k] = tracks[k];
		}
	}
}

void __thiscall CNodesTrafficInDoor_AddVehicle(CNodesTrafficInDoor *self, cAIVehicle *ai) {
	switch (Veh(cAIVehicle_GetVehicle(ai))->traffic.type) {
	case 0:
		self->lightAI[self->numLightAI++] = ai;
		break;
	case 1:
		self->mediumAI[self->numMediumAI++] = ai;
		break;
	case 2:
		self->heavyAI[self->numHeavyAI++] = ai;
		break;
	}
}

byte __thiscall CNodesTrafficInDoor_SectorHasTrack(CNodesTrafficInDoor *self, int track, int type) {
	cTrafficSector *list;
	int num;

	switch (type) {
	case TRAFFIC_HEAVY:
		list = self->heavy.list;
		num = self->heavy.num;
		break;
	case TRAFFIC_LIGHT:
		list = self->light.list;
		num = self->light.num;
		break;
	case TRAFFIC_MEDIUM:
		list = self->medium.list;
		num = self->medium.num;
		break;
	case TRAFFIC_HEAVY_LIGHT:
		list = self->heavyLight.list;
		num = self->heavyLight.num;
		break;
	case TRAFFIC_MEDIUM_LIGHT:
		list = self->mediumLight.list;
		num = self->mediumLight.num;
		break;
	default:
		/* the original leaves the type in both */
		list = (cTrafficSector *)(uintptr_t)type;
		num = type;
		break;
	}
	for (int i = 0; i < num; i++) {
		if (!list[i].visible) {
			continue;
		}
		for (int j = 0; j < list[i].numTracks; j++) {
			if (list[i].tracks[j] == track) {
				return 1;
			}
		}
	}
	return 0;
}

byte __thiscall CNodesTrafficInDoor_GetRespawnPoint(CNodesTrafficInDoor *self, cAIVehicle *ai, int *outType,
	vec3_t *out, int *outTrack, int *outNode) {
	switch (Veh(cAIVehicle_GetVehicle(ai))->traffic.type) {
	case 0:
		return CNodesTrafficInDoor_PickPoint(self, outType, out, outTrack, outNode, TRAFFIC_LIGHT, self->light.list,
			self->light.num, TRAFFIC_HEAVY_LIGHT, self->heavyLight.list, self->heavyLight.num,
			TRAFFIC_MEDIUM_LIGHT, self->mediumLight.list, self->mediumLight.num);
	case 1:
		return CNodesTrafficInDoor_PickPoint(self, outType, out, outTrack, outNode, TRAFFIC_MEDIUM,
			self->medium.list, self->medium.num, TRAFFIC_MEDIUM_LIGHT, self->mediumLight.list,
			self->mediumLight.num, 5, NULL, 0);
	case 2:
		return CNodesTrafficInDoor_PickPoint(self, outType, out, outTrack, outNode, TRAFFIC_HEAVY, self->heavy.list,
			self->heavy.num, TRAFFIC_HEAVY_LIGHT, self->heavyLight.list, self->heavyLight.num, 5, NULL, 0);
	}
	/* al of the original here: the top byte of the outNode argument */
	return (byte)((uintptr_t)outNode >> 24);
}

/* the visible sectors of a list and their nodes */
static int Sectors_Visible(const cTrafficSector *sectors, int num, int *index, int *nodes) {
	int n = 0;

	*nodes = 0;
	for (int i = 0; i < num; i++) {
		if (sectors[i].visible) {
			index[n++] = i;
			*nodes += sectors[i].numNodes;
		}
	}
	return n;
}

byte __thiscall CNodesTrafficInDoor_PickPoint(CNodesTrafficInDoor *self, int *outType, vec3_t *out, int *outTrack,
	int *outNode, int typeA, cTrafficSector *sectorsA, int numA, int typeB, cTrafficSector *sectorsB, int numB,
	int typeC, cTrafficSector *sectorsC, int numC) {
	int listA[20], listB[20], listC[20], sumA, sumB, sumC;
	int nA = Sectors_Visible(sectorsA, numA, listA, &sumA);
	int nB = Sectors_Visible(sectorsB, numB, listB, &sumB);
	int nC = Sectors_Visible(sectorsC, numC, listC, &sumC);
	const int *list;
	const cTrafficSector *sectors;
	int n, type;

	if (nA <= 0 && nB <= 0 && nC <= 0) {
		return 0;
	}
	int r = crt_rand() % (sumB + sumC + sumA);
	if (r < sumA) {
		type = typeA;
		list = listA;
		n = nA;
		sectors = sectorsA;
	} else if (r < sumB + sumA) {
		type = typeB;
		list = listB;
		n = nB;
		sectors = sectorsB;
	} else {
		type = typeC;
		list = listC;
		n = nC;
		sectors = sectorsC;
	}
	*outType = type;
	const cTrafficSector *s = &sectors[list[crt_rand() % n]];
	int j = crt_rand() % s->numTracks;
	int up = crt_rand() % 2;
	int numT = s->numTracks;
	const cTrafficTrack *tracks = self->base.tracks;
	for (int c = 0; c < numT; c++) {
		int track = s->tracks[j];
		const cTrafficTrack *t = &tracks[track];
		byte room = 1;
		if (t->density == TRAFFIC_DENSITY_NORMAL) {
			if (t->numVehicles >= g_MaxNumTrackNormalDensityAI) {
				room = 0;
			}
		} else if (t->density == TRAFFIC_DENSITY_LOW && t->numVehicles >= g_MaxNumTrackLowDensityAI) {
			room = 0;
		}
		if (!t->loop && room) {
			int node = CNodesTrafficInDoor_HiddenNode(self, track);
			self->base.vftable->RandomPointNear(&self->base, track, node, g_MaxDistAroundReposAI, out, outTrack,
				outNode);
			if (CNodesWorld_PointSeen(g_NodesWorld, out)) {
				*out = self->base.tracks[track].nodes[node];
			}
			return 1;
		}
		j = Sector_NextTrack(j, up, numT);
	}
	return 0;
}
