/*
 * AI/NodesTraffic.cpp and AI/NodesTrafficInDoor.cpp of the game: the traffic graph of a level. The tracks are the
 * dummies DM_Track<id>_<n> of the level (the info of the first node: loop, density, type, radius, vel). Outdoors the
 * nodes of all tracks go into a grid of cells (Global/GridElements.h, cGridElements<InfoTrafficNode>), which the
 * node searches walk ring by ring around the cell of the position. CNodesTrafficInDoor has no grid; it keeps the
 * tracks by the sectors of their nodes and puts the vehicles of the indoor levels on tracks of visible sectors.
 */
#ifndef SCRAP_NODESTRAFFIC_H
#define SCRAP_NODESTRAFFIC_H

#include "scrap/ai.h"
#include "scrap/dlist.h"

/* ---- Global/GridElements.h with InfoTrafficNode ---- */

/* a node of a track: what the grid cells hold */
typedef struct InfoTrafficNode {
	int track;			/* 0x0 */
	int node;			/* 0x4 */
} InfoTrafficNode;

typedef struct cTrafficCellNode {
	struct cTrafficCellNode *next;	/* 0x0 */
	struct cTrafficCellNode *prev;	/* 0x4 */
	InfoTrafficNode info;		/* 0x8 */
} cTrafficCellNode;
SD_SIZE(cTrafficCellNode, 0x10);

/* the nodes of a cell (Global/DList.h) */
typedef DLIST(cTrafficCellNode) cTrafficCell;
SD_SIZE(cTrafficCell, 0x18);

typedef struct cGridElements cGridElements;
typedef struct cGridElementsVtbl {
	cGridElements *(__thiscall *Delete)(cGridElements *self, byte flags);			/* 0x00 */
	byte(__thiscall *Add)(cGridElements *self, const vec3_t *p, const InfoTrafficNode *info);	/* 0x04 */
	byte(__thiscall *GetCellAt)(cGridElements *self, const vec3_t *p, cTrafficCell **out);	/* 0x08 */
	byte(__thiscall *GetCell)(cGridElements *self, const int *x, const int *y, const int *z, cTrafficCell **out);
	byte(__thiscall *Draw)(cGridElements *self, uint32_t color);				/* 0x10 */
	byte(__thiscall *CellCoords)(cGridElements *self, const vec3_t *p, int *x, int *y, int *z);	/* 0x14 */
	void(__thiscall *GetDivs)(cGridElements *self, int *nx, int *ny, int *nz);		/* 0x18 */
	vec3_t *(__thiscall *GetCellSize)(cGridElements *self, vec3_t *out);			/* 0x1c */
	byte(__thiscall *IsInside)(cGridElements *self, const vec3_t *p);			/* 0x20 */
	void(__thiscall *Free)(cGridElements *self);						/* 0x24 */
	int(__thiscall *CellIndex)(cGridElements *self, const vec3_t *p);			/* 0x28 */
} cGridElementsVtbl;
SD_OFFSET(cGridElementsVtbl, CellIndex, 0x28);

struct cGridElements {
	const cGridElementsVtbl *vftable;	/* 0x00 */
	cTrafficCell **cells;		/* 0x04 nx * ny * nz, NULL for an empty cell (Mem_Alloc) */
	int numCells;			/* 0x08 */
	vec3_t max;			/* 0x0c */
	vec3_t min;			/* 0x18 */
	int nx, ny, nz;			/* 0x24 */
	vec3_t cell;			/* 0x30 the size of a cell */
};
SD_OFFSET(cGridElements, nx, 0x24);
SD_SIZE(cGridElements, 0x3c);

#define CGRIDELEMENTS_VFTABLE 0x3a9ca4

cGridElements *__thiscall cGridElements_ctor(cGridElements *self, const vec3_t *max, const vec3_t *min, int nx,
	int ny, int nz);
/* the constructor of all instances, with the vtable (rva) of the instance */
cGridElements *cGridElements_Init(cGridElements *self, uint32_t vftable, const vec3_t *max, const vec3_t *min, int nx,
	int ny, int nz);
/* the cells emptied with clear and freed with their free nodes of nodeSize bytes, then the cell array */
void cGridElements_FreeCells(cGridElements *self, void(__fastcall *clear)(void *cell), uint32_t nodeSize);
void __fastcall cGridElements_dtor(cGridElements *self);
cGridElements *__thiscall cGridElements_Delete(cGridElements *self, byte flags);
/* the cells freed with their nodes, then the cell array */
void __thiscall cGridElements_Free(cGridElements *self);
/* the node into the cell of p (a CHECK out of the box); 1 */
byte __thiscall cGridElements_Add(cGridElements *self, const vec3_t *p, const InfoTrafficNode *info);
byte __thiscall cGridElements_GetCellAt(cGridElements *self, const vec3_t *p, cTrafficCell **out);
byte __thiscall cGridElements_GetCell(cGridElements *self, const int *x, const int *y, const int *z,
	cTrafficCell **out);
/* the lines of the cells; 0 without cells */
byte __thiscall cGridElements_Draw(cGridElements *self, uint32_t color);
/* the cell of p, each clamped to the last one (a CHECK out of the box); 1 */
byte __thiscall cGridElements_CellCoords(cGridElements *self, const vec3_t *p, int *x, int *y, int *z);
void __thiscall cGridElements_GetDivs(cGridElements *self, int *nx, int *ny, int *nz);
vec3_t *__thiscall cGridElements_GetCellSize(cGridElements *self, vec3_t *out);
byte __thiscall cGridElements_IsInside(cGridElements *self, const vec3_t *p);
/* the index of the cell of p, no check below 0, at most the last cell */
int __thiscall cGridElements_CellIndex(cGridElements *self, const vec3_t *p);

/* the DList.h instances: the cell (nodes of 0x10 bytes) and the error points of CNodesTraffic (0x18) */
void __thiscall TrafficCell_Add(cTrafficCell *cell, const InfoTrafficNode *info);
void __fastcall TrafficCell_Clear(cTrafficCell *cell);
void __fastcall TrafficErrors_Clear(void *list);

/* ---- CNodesTraffic ---- */

enum {
	TRAFFIC_HEAVY,
	TRAFFIC_LIGHT,
	TRAFFIC_MEDIUM,
	TRAFFIC_HEAVY_LIGHT,
	TRAFFIC_MEDIUM_LIGHT
};

enum { TRAFFIC_DENSITY_HIGH, TRAFFIC_DENSITY_NORMAL, TRAFFIC_DENSITY_LOW };

typedef struct cTrafficTrack {
	vec3_t *nodes;			/* 0x00 Mem_Alloc */
	int numNodes;			/* 0x04 */
	int type;			/* 0x08 TRAFFIC_* */
	byte loop;			/* 0x0c the last node goes on to the first */
	byte pad_d[3];
	float speed;			/* 0x10 vel of the info times 27.78 */
	float radius;			/* 0x14 */
	int id;				/* 0x18 the number of DM_Track<id>_<n> */
	int numVehicles;		/* 0x1c the vehicles on the track (AddVehicle, RemoveVehicle) */
	int density;			/* 0x20 TRAFFIC_DENSITY_* */
} cTrafficTrack;
SD_SIZE(cTrafficTrack, 0x24);

/* what GetInfo returns of a track */
typedef struct cTrafficTrackInfo {
	int numNodes;			/* 0x00 */
	int type;			/* 0x04 */
	byte loop;			/* 0x08 */
	byte pad_9[3];
	float speed;			/* 0x0c */
	float radius;			/* 0x10 */
	int id;				/* 0x14 */
} cTrafficTrackInfo;
SD_SIZE(cTrafficTrackInfo, 0x18);

/* a point where a track hits the level (AnalizeTraffic) */
typedef struct cTrafficErrorNode {
	struct cTrafficErrorNode *next;	/* 0x00 */
	struct cTrafficErrorNode *prev;	/* 0x04 */
	vec3_t pos;			/* 0x08 */
	float radius;			/* 0x14 */
} cTrafficErrorNode;
SD_SIZE(cTrafficErrorNode, 0x18);

typedef struct CNodesTraffic CNodesTraffic;
typedef struct CNodesTrafficVtbl {
	CNodesTraffic *(__thiscall *Delete)(CNodesTraffic *self, byte flags);		/* 0x00 */
	void(__thiscall *AddVehicle)(CNodesTraffic *self, int track);			/* 0x04 */
	void(__thiscall *RemoveVehicle)(CNodesTraffic *self, int track);		/* 0x08 */
	int(__thiscall *NumTracks)(CNodesTraffic *self);				/* 0x0c */
	int(__thiscall *NumByType)(CNodesTraffic *self, int type);			/* 0x10 */
	int(__thiscall *NumNodes)(CNodesTraffic *self, int track);			/* 0x14 */
	float(__thiscall *GetRadius)(CNodesTraffic *self, int track);			/* 0x18 */
	void(__thiscall *Init)(CNodesTraffic *self);					/* 0x1c */
	void(__thiscall *Load)(CNodesTraffic *self, byte fill);				/* 0x20 */
	byte(__thiscall *RandomNode)(CNodesTraffic *self, vec3_t *out, int type, const vec3_t *pos, float distMin,
		float distMax, int unused, const vec3_t *dir, float maxAngle, int *outTrack, int *outNode);	/* 0x24 */
	byte(__thiscall *GetDirs)(CNodesTraffic *self, vec3_t *prev, vec3_t *next, int track, int node);	/* 0x28 */
	byte(__thiscall *NextAway)(CNodesTraffic *self, vec3_t *out, int track, int node, int *outTrack,
		int *outNode);								/* 0x2c */
	byte(__thiscall *PrevAway)(CNodesTraffic *self, vec3_t *out, int track, int node, int *outTrack,
		int *outNode);								/* 0x30 */
	byte(__thiscall *GetNode)(CNodesTraffic *self, vec3_t *out, int track, int node);	/* 0x34 */
	cTrafficTrackInfo *(__thiscall *GetInfo)(CNodesTraffic *self, cTrafficTrackInfo *out, int track);	/* 0x38 */
	void(__thiscall *Draw)(CNodesTraffic *self);					/* 0x3c */
	void(__thiscall *Analize)(CNodesTraffic *self);					/* 0x40 */
	void(__thiscall *DrawTrack)(CNodesTraffic *self, int track);			/* 0x44 */
	void(__thiscall *BuildGrid)(CNodesTraffic *self);				/* 0x48 */
	void(__thiscall *FindNodes)(CNodesTraffic *self, const vec3_t *pos, int type, InfoTrafficNode *list, int max,
		int *count, float radius, float distMin, float distMax, byte soft, byte vis);	/* 0x4c */
	void(__thiscall *CollectCell)(CNodesTraffic *self, const vec3_t *pos, int type, cTrafficCell *cell,
		InfoTrafficNode *list, int max, int *count, float radius, float distMin, float distMax, byte vis,
		byte useMax);								/* 0x50 */
	void(__thiscall *SearchRings)(CNodesTraffic *self, const vec3_t *pos, int type, InfoTrafficNode *list,
		int max, int *count, float radius, float distMin, float distMax, byte soft, byte vis);	/* 0x54 */
	void(__thiscall *RandomPointNear)(CNodesTraffic *self, int track, int node, float maxDist, vec3_t *out,
		int *outTrack, int *outNode);						/* 0x58 */
	void(__thiscall *RandomPointAhead)(CNodesTraffic *self, int track, int node, const vec3_t *pos,
		float distMin, float distMax, int unused, const vec3_t *dir, float maxAngle, vec3_t *out, int *outTrack,
		int *outNode);								/* 0x5c */
} CNodesTrafficVtbl;
SD_OFFSET(CNodesTrafficVtbl, Init, 0x1c);
SD_OFFSET(CNodesTrafficVtbl, Analize, 0x40);
SD_OFFSET(CNodesTrafficVtbl, RandomPointAhead, 0x5c);

typedef DLIST(cTrafficErrorNode) cTrafficErrorList;

struct CNodesTraffic {
	const CNodesTrafficVtbl *vftable;	/* 0x00 */
	float cellSize;			/* 0x04 of the grid: 25000 outdoors, 12500 indoors */
	cGridElements *grid;		/* 0x08 NULL indoors */
	cTrafficTrack *tracks;		/* 0x0c Mem_Alloc */
	int *heavy;			/* 0x10 the tracks by type (Mem_Alloc) */
	int *medium;			/* 0x14 */
	int *light;			/* 0x18 */
	int *heavyLight;		/* 0x1c */
	int *mediumLight;		/* 0x20 */
	int numTracks;			/* 0x24 */
	int numHeavy;			/* 0x28 */
	int numMedium;			/* 0x2c */
	int numLight;			/* 0x30 */
	int numHeavyLight;		/* 0x34 */
	int numMediumLight;		/* 0x38 */
	int numNodes;			/* 0x3c of all tracks */
	vec3_t max;			/* 0x40 the box of the grid: of the visibility graph, outdoors up to DM_MapTop */
	vec3_t min;			/* 0x4c */
	cTrafficErrorList errors;	/* 0x58 */
};
SD_OFFSET(CNodesTraffic, numTracks, 0x24);
SD_OFFSET(CNodesTraffic, max, 0x40);
SD_OFFSET(CNodesTraffic, errors, 0x58);
SD_SIZE(CNodesTraffic, 0x70);

#define CNODESTRAFFIC_VFTABLE 0x3a9c40

/* the engine variables */
#define g_DistGetPVisTrafficAI GAME_DATA(float, 0x444cc4)	/* the searches end at this distance */
#define g_TrafficSearchDist GAME_DATA(float, 0x428000)		/* the copy of it the searches use */
#define g_MinDistNodeTrafficAI GAME_DATA(float, 0x444cac)	/* the step of the nodes Load fills in */
#define g_MaxNumTrackLowDensityAI GAME_DATA(int, 0x444cf4)
#define g_MaxNumTrackNormalDensityAI GAME_DATA(int, 0x444d24)
#define g_ShowNumTrafficAI GAME_DATA(int, 0x444cdc)		/* the track drawn, 0 for none */
#define g_MaxDistAroundReposAI GAME_DATA(float, 0x444930)
/* the default radius of the tracks by type */
#define g_HTrafficCapsule GAME_DATA(float, 0x448734)
#define g_MTrafficCapsule GAME_DATA(float, 0x448794)
#define g_NTrafficCapsule GAME_DATA(float, 0x448764)
#define g_HNTrafficCapsule GAME_DATA(float, 0x44877c)
#define g_MNTrafficCapsule GAME_DATA(float, 0x44874c)

/* CNodesTraffic_ctor and CNodesTrafficInDoor_ctor: ai.h */
void __fastcall CNodesTraffic_dtor(CNodesTraffic *self);
CNodesTraffic *__thiscall CNodesTraffic_Delete(CNodesTraffic *self, byte flags);
void __thiscall CNodesTraffic_AddVehicle(CNodesTraffic *self, int track);
void __thiscall CNodesTraffic_RemoveVehicle(CNodesTraffic *self, int track);
/* 4 bytes in the exe with int3 after them */
int __thiscall CNodesTraffic_NumTracks(CNodesTraffic *self);
int __thiscall CNodesTraffic_NumByType(CNodesTraffic *self, int type);
int __thiscall CNodesTraffic_NumNodes(CNodesTraffic *self, int track);
float __thiscall CNodesTraffic_GetRadius(CNodesTraffic *self, int track);
void __thiscall CNodesTraffic_Init(CNodesTraffic *self);
/* the tracks from the dummies; fill puts nodes between two dummies at least MinDistNodeTrafficAI apart */
void __thiscall CNodesTraffic_Load(CNodesTraffic *self, byte fill);
/* a node of the type found around pos (FindNodes) in the cone of maxAngle around dir, from a random start; the
 * node after it into outNode */
byte __thiscall CNodesTraffic_RandomNode(CNodesTraffic *self, vec3_t *out, int type, const vec3_t *pos, float distMin,
	float distMax, int unused, const vec3_t *dir, float maxAngle, int *outTrack, int *outNode);
/* the unit directions to the nodes 100 away before and after the node (0 0 0 for none); 0 for a bad node */
byte __thiscall CNodesTraffic_GetDirs(CNodesTraffic *self, vec3_t *prev, vec3_t *next, int track, int node);
/* the first node after (before) the node that is 100 or more away in one axis; 0 at the end of an open track */
byte __thiscall CNodesTraffic_NextAway(CNodesTraffic *self, vec3_t *out, int track, int node, int *outTrack,
	int *outNode);
byte __thiscall CNodesTraffic_PrevAway(CNodesTraffic *self, vec3_t *out, int track, int node, int *outTrack,
	int *outNode);
byte __thiscall CNodesTraffic_GetNode(CNodesTraffic *self, vec3_t *out, int track, int node);
cTrafficTrackInfo *__thiscall CNodesTraffic_GetInfo(CNodesTraffic *self, cTrafficTrackInfo *out, int track);
void __thiscall CNodesTraffic_Draw(CNodesTraffic *self);
void __thiscall CNodesTraffic_DrawTrack(CNodesTraffic *self, int track);
void __thiscall CNodesTraffic_BuildGrid(CNodesTraffic *self);
/* the nodes of the type between distMin and distMax from pos: the cell of pos, then the rings around it; soft goes
 * on past distMax until one is found, vis keeps only nodes the visibility graph sees with radius */
void __thiscall CNodesTraffic_FindNodes(CNodesTraffic *self, const vec3_t *pos, int type, InfoTrafficNode *list,
	int max, int *count, float radius, float distMin, float distMax, byte soft, byte vis);
void __thiscall CNodesTraffic_CollectCell(CNodesTraffic *self, const vec3_t *pos, int type, cTrafficCell *cell,
	InfoTrafficNode *list, int max, int *count, float radius, float distMin, float distMax, byte vis, byte useMax);
/* a random point between the node and the node 100 away after or before it (at most maxDist from the node) */
void __thiscall CNodesTraffic_RandomPointNear(CNodesTraffic *self, int track, int node, float maxDist, vec3_t *out,
	int *outTrack, int *outNode);
/* such a point between distMin and distMax from pos in the cone of maxAngle around dir, else the next node */
void __thiscall CNodesTraffic_RandomPointAhead(CNodesTraffic *self, int track, int node, const vec3_t *pos,
	float distMin, float distMax, int unused, const vec3_t *dir, float maxAngle, vec3_t *out, int *outTrack,
	int *outNode);

/* ---- CNodesTrafficInDoor ---- */

/* the nodes of the tracks of one type in one sector */
typedef struct cTrafficSector {
	int sector;			/* 0x00 */
	InfoTrafficNode *nodes;		/* 0x04 Mem_Alloc */
	int numNodes;			/* 0x08 */
	int *tracks;			/* 0x0c the tracks of the nodes, each once (Mem_Alloc) */
	int numTracks;			/* 0x10 */
	byte visible;			/* 0x14 Sector_Visible at the last update */
	byte pad_15[3];
} cTrafficSector;
SD_SIZE(cTrafficSector, 0x18);

typedef struct cTrafficSectorList {
	cTrafficSector *list;		/* 0x0 */
	int num;			/* 0x4 */
} cTrafficSectorList;

/* the sectors of a type that came into view at the last update and their nodes */
typedef struct cTrafficSeen {
	int index[20];			/* 0x00 into the sector list */
	int num;			/* 0x50 */
	int numNodes;			/* 0x54 */
} cTrafficSeen;
SD_SIZE(cTrafficSeen, 0x58);

typedef struct CNodesTrafficInDoor {
	CNodesTraffic base;		/* 0x000 */
	cTrafficSectorList heavy;	/* 0x070 the sectors by the type of the tracks */
	cTrafficSectorList medium;	/* 0x078 */
	cTrafficSectorList light;	/* 0x080 */
	cTrafficSectorList heavyLight;	/* 0x088 */
	cTrafficSectorList mediumLight;	/* 0x090 */
	cAIVehicle *heavyAI[100];	/* 0x098 the traffic vehicles by the class of the vehicle (2) */
	int numHeavyAI;			/* 0x228 */
	cAIVehicle *mediumAI[250];	/* 0x22c (1) */
	int numMediumAI;		/* 0x614 */
	cAIVehicle *lightAI[500];	/* 0x618 (0) */
	int numLightAI;			/* 0xde8 */
	cTrafficSeen seenHeavy;		/* 0xdec */
	cTrafficSeen seenMedium;	/* 0xe44 */
	cTrafficSeen seenLight;		/* 0xe9c */
	cTrafficSeen seenHeavyLight;	/* 0xef4 */
	cTrafficSeen seenMediumLight;	/* 0xf4c */
} CNodesTrafficInDoor;
SD_OFFSET(CNodesTrafficInDoor, heavyAI, 0x98);
SD_OFFSET(CNodesTrafficInDoor, numLightAI, 0xde8);
SD_OFFSET(CNodesTrafficInDoor, seenMediumLight, 0xf4c);
SD_SIZE(CNodesTrafficInDoor, 0xfa4);

#define CNODESTRAFFICINDOOR_VFTABLE 0x3aa784
#define TRAFFICINDOOR_FRAME 0x0a0c20

void __fastcall CNodesTrafficInDoor_dtor(CNodesTrafficInDoor *self);
CNodesTrafficInDoor *__thiscall CNodesTrafficInDoor_Delete(CNodesTrafficInDoor *self, byte flags);
/* the engine callback 2 of the indoor levels: UpdateVehicles of g_NodesTrafficInDoor; 1 */
byte TrafficInDoor_Frame(void);
/* the vehicles waiting for a place (state 9) put on tracks of sectors that came into view */
void __thiscall CNodesTrafficInDoor_UpdateVehicles(CNodesTrafficInDoor *self);
void __thiscall CNodesTrafficInDoor_Respawn(CNodesTrafficInDoor *self, cAIVehicle **ai, int numAI, int typeA,
	cTrafficSector *sectorsA, int numA, int *seenA, int *numSeenA, int *nodesA, byte updateA, int typeB,
	cTrafficSector *sectorsB, int numB, int *seenB, int *numSeenB, int *nodesB, byte updateB, int typeC,
	cTrafficSector *sectorsC, int numC, int *seenC, int *numSeenC, int *nodesC, byte updateC);
/* the node before the first node of the track the camera sees (0 if that is the last one) */
int __thiscall CNodesTrafficInDoor_HiddenNode(CNodesTrafficInDoor *self, int track);
/* the sector lists of the tracks */
void __fastcall CNodesTrafficInDoor_InitSectors(CNodesTrafficInDoor *self);
void __stdcall TrafficSectors_Add(cTrafficSector *list, int *num, int sector);
void __thiscall CNodesTrafficInDoor_Init(CNodesTrafficInDoor *self);
void __stdcall TrafficSectors_SetTracks(cTrafficSector *list, int num);
/* the traffic vehicle into the list of the class of its vehicle */
void __thiscall CNodesTrafficInDoor_AddVehicle(CNodesTrafficInDoor *self, cAIVehicle *ai);
/* a visible sector of the type has the track */
byte __thiscall CNodesTrafficInDoor_SectorHasTrack(CNodesTrafficInDoor *self, int track, int type);
/* a point near a hidden node of a visible sector for the vehicle of the AI, by the class of the vehicle */
byte __thiscall CNodesTrafficInDoor_GetRespawnPoint(CNodesTrafficInDoor *self, cAIVehicle *ai, int *outType,
	vec3_t *out, int *outTrack, int *outNode);
byte __thiscall CNodesTrafficInDoor_PickPoint(CNodesTrafficInDoor *self, int *outType, vec3_t *out, int *outTrack,
	int *outNode, int typeA, cTrafficSector *sectorsA, int numA, int typeB, cTrafficSector *sectorsB, int numB,
	int typeC, cTrafficSector *sectorsC, int numC);

/* the exe functions of other modules */
/* AI/AIVehicle.cpp (include/scrap/aivehicle.h): the vehicle entity of the AI, the internal state of its move (0
 * stopped), the AI put on the track */
void *__thiscall cAIVehicle_GetVehicle(cAIVehicle *ai);
int __thiscall cAIVehicle_GetInternState(cAIVehicle *ai);
byte __thiscall cAIVehicle_PlaceOnTrack(cAIVehicle *ai, int type, const vec3_t *pos, int track, int node);
#define VEHICLE_CLASS(v) (*(int *)((byte *)(v) + 0x5e8))	/* 0 light, 1 medium, 2 heavy */

#endif
