/*
 * World/CollisionGrid.cpp of the game: the entity grid of the world (g_pWorld->grid). The box of the floor is cut
 * into cells; every entity with active is linked into the cell of its position (Nto1Links: the cell is the head,
 * the node of the entity at +0x34), the others outside. The queries walk the cells of a box or of the boxes along a
 * segment, one query at a time. A segment query marks the cells it has seen with a stamp, so the boxes along it give
 * each entity once. Also here: the collision masks of the entity tests, the debug draws of the cells and the world
 * dummies, the dummy search of Scrap.StartDummySearch and the entity lists of the world (cEntityList).
 */
#ifndef SCRAP_COLLISIONGRID_H
#define SCRAP_COLLISIONGRID_H

#include "scrap/gvar.h"
#include "scrap/entity.h"
#include "scrap/pyfunction.h"

typedef struct cCollisionGrid {
	vec3_t min;			/* 0x00 */
	vec3_t max;			/* 0x0c */
	vec3_t cell;			/* 0x18 the size of a cell */
	int nx, ny, nz;			/* 0x24 the cells per axis */
	Nto1Node outside;		/* 0x30 the head of the entities out of the box */
	Nto1Node *cells;		/* 0x40 new[] of nx * ny * nz heads, target the cell itself */
	Nto1Node *cur;			/* 0x44 the node of the query, NULL at the end */
	byte *visited;			/* 0x48 the stamp of each cell */
	byte stamp;			/* 0x4c of the segment query; 0xff wraps to 1 with the stamps cleared */
	byte pad_4d[3];
	int start[3];			/* 0x50 the first cell of the box query */
	int end[3];			/* 0x5c the last cell */
	int pos[3];			/* 0x68 the cell of the query */
	vec3_t segPos;			/* 0x74 the low corner of the box of the segment query */
	vec3_t segStep;			/* 0x80 one cell along the segment */
	vec3_t ext;			/* 0x8c the size of the box along the segment */
	int steps;			/* 0x98 boxes left along the segment, 0 for a box query */
} cCollisionGrid;
SD_OFFSET(cCollisionGrid, outside, 0x30);
SD_OFFSET(cCollisionGrid, stamp, 0x4c);
SD_OFFSET(cCollisionGrid, segPos, 0x74);
SD_OFFSET(cCollisionGrid, steps, 0x98);
SD_SIZE(cCollisionGrid, 0x9c);

/* the fields of the entities the grid uses */
#define Entity_Pos(e) (&((cEntity *)(e))->pos)
#define Entity_GridNode(e) (&((cEntity *)(e))->gridNode)	/* target the cell, data the entity */

/* the engine variables (Gvars) */
extern cGVar GVar_GridUseTopLimit;
#define g_GridUseTopLimit (GVar_GridUseTopLimit.value.i)
extern cGVar GVar_TopLimit;
#define g_TopLimit (GVar_TopLimit.value.f)
extern cGVar GVar_ShowDummy;
#define g_ShowDummy (GVar_ShowDummy.value.s)	/* the dummies drawn (a wildcard pattern), "" for none */

/* the destructor of a cell (Nto1Links.h, rva) for the new[] of the cells */
#define NTO1NODE_DTOR 0x0eeb90
#define GRIDCELL_CTOR 0x1a6700

#define Grid_IsOk(grid) (((const cCollisionGrid *)(grid))->cells != NULL)

/* a cell head: an empty ring (the constructor of new[]) */
Nto1Node *GridCell_ctor(Nto1Node *self);

/* the grid is used as void * (the byte array of cWorld) */
void *Grid_ctor(void *grid);
void Grid_dtor(void *grid);
/* the cells freed, the box cleared */
void Grid_Free(void *grid);
void Grid_Init(void *grid, const vec3_t *min, const vec3_t *max, const vec3_t *cell);
/* the grid made again with the cells of size; the top of the box is TopLimit with GridUseTopLimit */
void Grid_Set(void *grid, const vec3_t *size);
/* the entity into the cell of its position (none without active) */
void Grid_Add(void *grid, cEntity *e);
/* the first entity of cell x y z (outside of the box: the outside list); NULL for an empty cell or one the segment
 * query has seen */
cEntity *Grid_GetCell(void *grid, int x, int y, int z);
cEntity *Grid_GetCellAt(void *grid, const vec3_t *pos);
/* the next entity of the cell of the query, NULL at its end */
cEntity *Grid_NextInCell(void *grid);
/* the next cell of the box query; 1 after the last */
byte Grid_StepCell(void *grid);
/* the stamp of a new segment query */
void Grid_NewQuery(void *grid);
/* the entities near the segment, ext the size of the box moved along it */
cEntity *Grid_FirstOnSegment(void *grid, const vec3_t *from, const vec3_t *to, const vec3_t *ext);
cEntity *Grid_NextOnSegment(void *grid);
cEntity *Grid_FirstInBox(void *grid, const vec3_t *min, const vec3_t *max);
cEntity *Grid_NextInBox(void *grid);
void Grid_EndFrame(void *grid);
/* pos with x and z clamped into the box, y not below its bottom; returns out */
vec3_t *Grid_ClampPos(void *grid, vec3_t *out, const vec3_t *pos);
/* the debug draw of the cells, the color by the stamp */
void Grid_Draw(void *grid);

/* the world dummies of ShowDummy drawn with a line up to the top of the floor box */
void Dummies_Draw(void);
/* the next dummy of the search of DummySearch_Start: position and Euler angles; 0 at the end */
byte DummySearch_NextEuler(char *buf, int size, vec3_t *pos, vec3_t *ang);

/* the entity lists of g_pWorld->lists: the full layout of cEntityList (entity.h) */
typedef struct cEntityListData {
	char *name;			/* 0x00 string object */
	cPy_Function onDeath;		/* 0x04 */
	cPy_Function onDamage;		/* 0x18 */
	int (*func)(void);		/* 0x2c returns 0 */
	Nto1Node ring;			/* 0x30 head of the entities (data), target the list */
	int *numElems;			/* 0x40 */
} cEntityListData;
SD_OFFSET(cEntityListData, ring, 0x30);
SD_SIZE(cEntityListData, 0x44);

#define CENTITYLIST_FUNC 0x1a7f10

/* the entity of the list nearest to pos (the floor levels apart count 20000 each), not exclude; the distance into
 * dist */
cEntity *cEntityList_Nearest(void *list, const vec3_t *pos, cEntity *exclude, float *dist);

#endif
