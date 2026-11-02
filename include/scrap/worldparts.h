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
	struct cSchedNode *next;	/* 0x0 */
	struct cSchedNode *prev;	/* 0x4 */
	struct cScheduledFunc *func;	/* 0x8 */
} cSchedNode;
SD_SIZE(cSchedNode, 0xc);

typedef DLIST(cSchedNode) cSchedList;

typedef struct cScheduledFuncStack {
	void *vftable;			/* 0x00 */
	cSchedList list;		/* 0x04 */
} cScheduledFuncStack;
SD_SIZE(cScheduledFuncStack, 0x1c);

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

#endif
