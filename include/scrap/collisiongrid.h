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
#include "scrap/worldparts.h"

/* the fields of the entities the grid uses */
#define Entity_Pos(e) (&((cEntity *)(e))->pos)
#define Entity_GridNode(e) (&((cEntity *)(e))->gridNode)	/* target the cell, data the entity */

extern cGVar GVar_TopLimit;
#define g_TopLimit (GVar_TopLimit.value.f)

/* the destructor of a cell (Nto1Links.h, rva) for the new[] of the cells */
#define NTO1NODE_DTOR 0x0eeb90

#define Grid_IsOk(grid) (((const cCollisionGrid *)(grid))->cells != NULL)

/* the grid is used as void * (the byte array of cWorld) */
void *Grid_ctor(void *grid);
void Grid_dtor(void *grid);
void Grid_Init(void *grid, const vec3_t *min, const vec3_t *max, const vec3_t *cell);
/* the grid made again with the cells of size; the top of the box is TopLimit with GridUseTopLimit */
void Grid_Set(void *grid, const vec3_t *size);
/* the entity into the cell of its position (none without active) */
void Grid_Add(void *grid, cEntity *e);
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

/* the entity lists of g_pWorld->lists (cEntityList of entity.h) */
typedef cEntityList cEntityListData;


/* the entity of the list nearest to pos (the floor levels apart count 20000 each), not exclude; the distance into
 * dist */
cEntity *cEntityList_Nearest(void *list, const vec3_t *pos, cEntity *exclude, float *dist);

#endif
