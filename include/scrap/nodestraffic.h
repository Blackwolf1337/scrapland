/*
 * AI/NodesTraffic.cpp and AI/NodesTrafficInDoor.cpp of the game: the traffic graph of a level. The tracks are the
 * dummies DM_Track<id>_<n> of the level (the info of the first node: loop, density, type, radius, vel). Outdoors the
 * nodes of all tracks go into a grid of cells (Global/GridElements.h, cGridElements<InfoTrafficNode>), which the
 * node searches walk ring by ring around the cell of the position. CNodesTrafficInDoor has no grid; it keeps the
 * tracks by the sectors of their nodes and puts the vehicles of the indoor levels on tracks of visible sectors.
 */
#ifndef SCRAP_NODESTRAFFIC_H
#define SCRAP_NODESTRAFFIC_H

#include "scrap/gvar.h"
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
	cGridElements *(*Delete)(cGridElements *self, byte flags);			/* 0x00 */
	/* the element: an InfoTrafficNode, a pointer to the pos of a point of visibility in the grid of CNodesWorldSup */
	byte(*Add)(cGridElements *self, const vec3_t *p, const void *data);		/* 0x04 */
	byte(*GetCellAt)(cGridElements *self, const vec3_t *p, cTrafficCell **out);	/* 0x08 */
	byte(*GetCell)(cGridElements *self, const int *x, const int *y, const int *z, cTrafficCell **out);
	byte(*Draw)(cGridElements *self, uint32_t color);				/* 0x10 */
	byte(*CellCoords)(cGridElements *self, const vec3_t *p, int *x, int *y, int *z);	/* 0x14 */
	void(*GetDivs)(cGridElements *self, int *nx, int *ny, int *nz);		/* 0x18 */
	vec3_t *(*GetCellSize)(cGridElements *self, vec3_t *out);			/* 0x1c */
	byte(*IsInside)(cGridElements *self, const vec3_t *p);			/* 0x20 */
	void(*Free)(cGridElements *self);						/* 0x24 */
	int(*CellIndex)(cGridElements *self, const vec3_t *p);			/* 0x28 */
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
typedef cGridElements cGridElements_InfoTrafficNode;

extern const cGridElementsVtbl cGridElements_InfoTrafficNode_vftable;
#define CGRIDELEMENTS_VFTABLE (&cGridElements_InfoTrafficNode_vftable)

cGridElements *cGridElements_ctor(cGridElements *self, const vec3_t *max, const vec3_t *min, int nx,
	int ny, int nz);
/* the constructor of all instances, with the vtable (rva) of the instance */
cGridElements *cGridElements_Init(cGridElements *self, const cGridElementsVtbl *vftable, const vec3_t *max, const vec3_t *min, int nx,
	int ny, int nz);
/* the cells emptied with clear and freed with their free nodes of nodeSize bytes, then the cell array */
void cGridElements_FreeCells(cGridElements *self, void(*clear)(void *cell), uint32_t nodeSize);
cGridElements *cGridElements_Delete(cGridElements *self, byte flags);
/* the cells freed with their nodes, then the cell array */
void cGridElements_Free(cGridElements *self);
/* the node into the cell of p (a CHECK out of the box); 1 */
byte cGridElements_Add(cGridElements *self, const vec3_t *p, const void *data);
byte cGridElements_GetCellAt(cGridElements *self, const vec3_t *p, cTrafficCell **out);
byte cGridElements_GetCell(cGridElements *self, const int *x, const int *y, const int *z,
	cTrafficCell **out);
/* the lines of the cells; 0 without cells */
byte cGridElements_Draw(cGridElements *self, uint32_t color);
/* the cell of p, each clamped to the last one (a CHECK out of the box); 1 */
byte cGridElements_CellCoords(cGridElements *self, const vec3_t *p, int *x, int *y, int *z);
void cGridElements_GetDivs(cGridElements *self, int *nx, int *ny, int *nz);
vec3_t *cGridElements_GetCellSize(cGridElements *self, vec3_t *out);
byte cGridElements_IsInside(cGridElements *self, const vec3_t *p);
/* the index of the cell of p, no check below 0, at most the last cell */
int cGridElements_CellIndex(cGridElements *self, const vec3_t *p);

/* the DList.h instances: the cell (nodes of 0x10 bytes) and the error points of CNodesTraffic (0x18) */
void TrafficCell_Add(cTrafficCell *cell, const InfoTrafficNode *info);
void TrafficCell_Clear(cTrafficCell *cell);
void TrafficErrors_Clear(void *list);

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
	CNodesTraffic *(*Delete)(CNodesTraffic *self, byte flags);		/* 0x00 */
	void(*AddVehicle)(CNodesTraffic *self, int track);			/* 0x04 */
	void(*RemoveVehicle)(CNodesTraffic *self, int track);		/* 0x08 */
	int(*NumTracks)(CNodesTraffic *self);				/* 0x0c */
	int(*NumByType)(CNodesTraffic *self, int type);			/* 0x10 */
	int(*NumNodes)(CNodesTraffic *self, int track);			/* 0x14 */
	float(*GetRadius)(CNodesTraffic *self, int track);			/* 0x18 */
	void(*Init)(CNodesTraffic *self);					/* 0x1c */
	void(*Load)(CNodesTraffic *self, byte fill);				/* 0x20 */
	byte(*RandomNode)(CNodesTraffic *self, vec3_t *out, int type, const vec3_t *pos, float distMin,
		float distMax, int unused, const vec3_t *dir, float maxAngle, int *outTrack, int *outNode);	/* 0x24 */
	byte(*GetDirs)(CNodesTraffic *self, vec3_t *prev, vec3_t *next, int track, int node);	/* 0x28 */
	byte(*NextAway)(CNodesTraffic *self, vec3_t *out, int track, int node, int *outTrack,
		int *outNode);								/* 0x2c */
	byte(*PrevAway)(CNodesTraffic *self, vec3_t *out, int track, int node, int *outTrack,
		int *outNode);								/* 0x30 */
	byte(*GetNode)(CNodesTraffic *self, vec3_t *out, int track, int node);	/* 0x34 */
	cTrafficTrackInfo *(*GetInfo)(CNodesTraffic *self, cTrafficTrackInfo *out, int track);	/* 0x38 */
	void(*Draw)(CNodesTraffic *self);					/* 0x3c */
	void(*Analyze)(CNodesTraffic *self);					/* 0x40 */
	void(*DrawTrack)(CNodesTraffic *self, int track);			/* 0x44 */
	void(*BuildGrid)(CNodesTraffic *self);				/* 0x48 */
	void(*FindNodes)(CNodesTraffic *self, const vec3_t *pos, int type, InfoTrafficNode *list, int max,
		int *count, float radius, float distMin, float distMax, byte soft, byte vis);	/* 0x4c */
	void(*CollectCell)(CNodesTraffic *self, const vec3_t *pos, int type, cTrafficCell *cell,
		InfoTrafficNode *list, int max, int *count, float radius, float distMin, float distMax, byte vis,
		byte useMax);								/* 0x50 */
	void(*SearchRings)(CNodesTraffic *self, const vec3_t *pos, int type, InfoTrafficNode *list,
		int max, int *count, float radius, float distMin, float distMax, byte soft, byte vis);	/* 0x54 */
	void(*RandomPointNear)(CNodesTraffic *self, int track, int node, float maxDist, vec3_t *out,
		int *outTrack, int *outNode);						/* 0x58 */
	void(*RandomPointAhead)(CNodesTraffic *self, int track, int node, const vec3_t *pos,
		float distMin, float distMax, int unused, const vec3_t *dir, float maxAngle, vec3_t *out, int *outTrack,
		int *outNode);								/* 0x5c */
} CNodesTrafficVtbl;
SD_OFFSET(CNodesTrafficVtbl, Init, 0x1c);
SD_OFFSET(CNodesTrafficVtbl, Analyze, 0x40);
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

extern const CNodesTrafficVtbl CNodesTraffic_vftable;
#define CNODESTRAFFIC_VFTABLE (&CNodesTraffic_vftable)

/* the engine variables */
extern cGVar GVar_DistGetPVisTrafficAI;
#define g_DistGetPVisTrafficAI (GVar_DistGetPVisTrafficAI.value.f)	/* the searches end at this distance */
extern float g_TrafficSearchDist;		/* the copy of it the searches use */
extern cGVar GVar_MinDistNodeTrafficAI;
#define g_MinDistNodeTrafficAI (GVar_MinDistNodeTrafficAI.value.f)	/* the step of the nodes Load fills in */
extern cGVar GVar_MaxNumTrackLowDensityAI;
#define g_MaxNumTrackLowDensityAI (GVar_MaxNumTrackLowDensityAI.value.i)
extern cGVar GVar_MaxNumTrackNormalDensityAI;
#define g_MaxNumTrackNormalDensityAI (GVar_MaxNumTrackNormalDensityAI.value.i)
extern cGVar GVar_ShowNumTrafficAI;
#define g_ShowNumTrafficAI (GVar_ShowNumTrafficAI.value.i)		/* the track drawn, 0 for none */
extern cGVar GVar_MaxDistAroundReposAI;
#define g_MaxDistAroundReposAI (GVar_MaxDistAroundReposAI.value.f)
/* the default radius of the tracks by type */
extern cGVar GVar_HTrafficCapsule;
#define g_HTrafficCapsule (GVar_HTrafficCapsule.value.f)
extern cGVar GVar_MTrafficCapsule;
#define g_MTrafficCapsule (GVar_MTrafficCapsule.value.f)
extern cGVar GVar_NTrafficCapsule;
#define g_NTrafficCapsule (GVar_NTrafficCapsule.value.f)
extern cGVar GVar_HNTrafficCapsule;
#define g_HNTrafficCapsule (GVar_HNTrafficCapsule.value.f)
extern cGVar GVar_MNTrafficCapsule;
#define g_MNTrafficCapsule (GVar_MNTrafficCapsule.value.f)

/* CNodesTraffic_ctor and CNodesTrafficInDoor_ctor: ai.h */
void CNodesTraffic_dtor(CNodesTraffic *self);
CNodesTraffic *CNodesTraffic_Delete(CNodesTraffic *self, byte flags);
void CNodesTraffic_AddVehicle(CNodesTraffic *self, int track);
void CNodesTraffic_RemoveVehicle(CNodesTraffic *self, int track);
/* 4 bytes in the exe with int3 after them */
int CNodesTraffic_NumTracks(CNodesTraffic *self);
int CNodesTraffic_NumByType(CNodesTraffic *self, int type);
int CNodesTraffic_NumNodes(CNodesTraffic *self, int track);
float CNodesTraffic_GetRadius(CNodesTraffic *self, int track);
void CNodesTraffic_Init(CNodesTraffic *self);
/* the tracks from the dummies; fill puts nodes between two dummies at least MinDistNodeTrafficAI apart */
void CNodesTraffic_Load(CNodesTraffic *self, byte fill);
/* a node of the type found around pos (FindNodes) in the cone of maxAngle around dir, from a random start; the
 * node after it into outNode */
byte CNodesTraffic_RandomNode(CNodesTraffic *self, vec3_t *out, int type, const vec3_t *pos, float distMin,
	float distMax, int unused, const vec3_t *dir, float maxAngle, int *outTrack, int *outNode);
/* the unit directions to the nodes 100 away before and after the node (0 0 0 for none); 0 for a bad node */
byte CNodesTraffic_GetDirs(CNodesTraffic *self, vec3_t *prev, vec3_t *next, int track, int node);
/* the first node after (before) the node that is 100 or more away in one axis; 0 at the end of an open track */
byte CNodesTraffic_NextAway(CNodesTraffic *self, vec3_t *out, int track, int node, int *outTrack,
	int *outNode);
byte CNodesTraffic_PrevAway(CNodesTraffic *self, vec3_t *out, int track, int node, int *outTrack,
	int *outNode);
byte CNodesTraffic_GetNode(CNodesTraffic *self, vec3_t *out, int track, int node);
cTrafficTrackInfo *CNodesTraffic_GetInfo(CNodesTraffic *self, cTrafficTrackInfo *out, int track);
void CNodesTraffic_Draw(CNodesTraffic *self);
/* vtable slot 0x40: the tracks loaded without nodes between the dummies, their points above the graph (outdoors) or
 * in the level into errors and <level>/AI/AnalisisTraffic.txt (writes a file), with the folded tracks; then loaded
 * again with the nodes */
void CNodesTraffic_Analyze(CNodesTraffic *self);
void CNodesTraffic_DrawTrack(CNodesTraffic *self, int track);
void CNodesTraffic_BuildGrid(CNodesTraffic *self);
void CNodesTraffic_BuildGridNop(CNodesTraffic *self);
void CNodesTraffic_SearchRings(CNodesTraffic *self, const vec3_t *pos, int type, InfoTrafficNode *list, int max, int *count,
	float radius, float distMin, float distMax, byte soft, byte vis);
/* the nodes of the type between distMin and distMax from pos: the cell of pos, then the rings around it; soft goes
 * on past distMax until one is found, vis keeps only nodes the visibility graph sees with radius */
void CNodesTraffic_FindNodes(CNodesTraffic *self, const vec3_t *pos, int type, InfoTrafficNode *list,
	int max, int *count, float radius, float distMin, float distMax, byte soft, byte vis);
void CNodesTraffic_CollectCell(CNodesTraffic *self, const vec3_t *pos, int type, cTrafficCell *cell,
	InfoTrafficNode *list, int max, int *count, float radius, float distMin, float distMax, byte vis, byte useMax);
/* a random point between the node and the node 100 away after or before it (at most maxDist from the node) */
void CNodesTraffic_RandomPointNear(CNodesTraffic *self, int track, int node, float maxDist, vec3_t *out,
	int *outTrack, int *outNode);
/* such a point between distMin and distMax from pos in the cone of maxAngle around dir, else the next node */
void CNodesTraffic_RandomPointAhead(CNodesTraffic *self, int track, int node, const vec3_t *pos,
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

extern const CNodesTrafficVtbl CNodesTrafficInDoor_vftable;
#define CNODESTRAFFICINDOOR_VFTABLE (&CNodesTrafficInDoor_vftable)
#define TRAFFICINDOOR_FRAME 0x0a0c20

void CNodesTrafficInDoor_dtor(CNodesTrafficInDoor *self);
CNodesTraffic *CNodesTrafficInDoor_Delete(CNodesTraffic *base, byte flags);
/* the engine callback 2 of the indoor levels: UpdateVehicles of g_NodesTrafficInDoor; 1 */
byte TrafficInDoor_Frame(void);
/* the vehicles waiting for a place (state 9) put on tracks of sectors that came into view */
void CNodesTrafficInDoor_UpdateVehicles(CNodesTrafficInDoor *self);
void CNodesTrafficInDoor_Respawn(CNodesTrafficInDoor *self, cAIVehicle **ai, int numAI, int typeA,
	cTrafficSector *sectorsA, int numA, int *seenA, int *numSeenA, int *nodesA, byte updateA, int typeB,
	cTrafficSector *sectorsB, int numB, int *seenB, int *numSeenB, int *nodesB, byte updateB, int typeC,
	cTrafficSector *sectorsC, int numC, int *seenC, int *numSeenC, int *nodesC, byte updateC);
/* the node before the first node of the track the camera sees (0 if that is the last one) */
int CNodesTrafficInDoor_HiddenNode(CNodesTrafficInDoor *self, int track);
/* the sector lists of the tracks */
void CNodesTrafficInDoor_InitSectors(CNodesTrafficInDoor *self);
void TrafficSectors_Add(cTrafficSector *list, int *num, int sector);
void CNodesTrafficInDoor_Init(CNodesTraffic *elem);
/* vtable slot 0x48, empty (the slot 0x3c is CNodesTraffic_Draw) */
void CNodesTrafficInDoor_BuildGrid(CNodesTrafficInDoor *self);
void TrafficSectors_SetTracks(cTrafficSector *list, int num);
/* the traffic vehicle into the list of the class of its vehicle */
void CNodesTrafficInDoor_AddVehicle(CNodesTrafficInDoor *self, cAIVehicle *ai);
/* a visible sector of the type has the track */
byte CNodesTrafficInDoor_SectorHasTrack(CNodesTrafficInDoor *self, int track, int type);
/* a point near a hidden node of a visible sector for the vehicle of the AI, by the class of the vehicle */
byte CNodesTrafficInDoor_GetRespawnPoint(CNodesTrafficInDoor *self, cAIVehicle *ai, int *outType,
	vec3_t *out, int *outTrack, int *outNode);
byte CNodesTrafficInDoor_PickPoint(CNodesTrafficInDoor *self, int *outType, vec3_t *out, int *outTrack,
	int *outNode, int typeA, cTrafficSector *sectorsA, int numA, int typeB, cTrafficSector *sectorsB, int numB,
	int typeC, cTrafficSector *sectorsC, int numC);

/* the exe functions of other modules */
/* AI/AIVehicle.cpp (include/scrap/aivehicle.h): the vehicle entity of the AI, the internal state of its move (0
 * stopped), the AI put on the track */
void *cAIVehicle_GetVehicle(cAIVehicle *ai);
int cAIVehicle_GetInternState(cAIVehicle *ai);
byte cAIVehicle_PlaceOnTrack(cAIVehicle *ai, int type, const vec3_t *pos, int track, int node);

#endif
