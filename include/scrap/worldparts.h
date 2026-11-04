/*
 * The objects embedded in cWorld (include/scrap/game.h) whose own headers need game.h: the scheduled function stack
 * (scheduler.h) and the entity grid (collisiongrid.h).
 */
#ifndef SCRAP_WORLDPARTS_H
#define SCRAP_WORLDPARTS_H

#include "scrap/dlist.h"
#include "scrap/mathlib.h"
#include "scrap/nto1links.h"

typedef struct cSchedNode {
	struct cSchedNode *next;
	struct cSchedNode *prev;
	struct cScheduledFunc *func;
} cSchedNode;

typedef DLIST(cSchedNode) cSchedList;

typedef struct cScheduledFuncStack {
	cSchedList list;
} cScheduledFuncStack;

typedef struct cCollisionGrid {
	vec3_t min;
	vec3_t max;
	vec3_t cell;			/* the size of a cell */
	int nx, ny, nz;			/* the cells per axis */
	Nto1Node outside;		/* the head of the entities out of the box */
	Nto1Node *cells;		/* new[] of nx * ny * nz heads, target the cell itself */
	Nto1Node *cur;			/* the node of the query, NULL at the end */
	byte *visited;			/* the stamp of each cell */
	byte stamp;			/* of the segment query; 0xff wraps to 1 with the stamps cleared */
	byte pad_4d[3];
	int start[3];			/* the first cell of the box query */
	int end[3];			/* the last cell */
	int pos[3];			/* the cell of the query */
	vec3_t segPos;			/* the low corner of the box of the segment query */
	vec3_t segStep;			/* one cell along the segment */
	vec3_t ext;			/* the size of the box along the segment */
	int steps;			/* boxes left along the segment, 0 for a box query */
} cCollisionGrid;

#endif
