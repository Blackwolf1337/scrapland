/*
 * Global/DList.h: the doubly linked ring of the engine, with its first node, an iterator and a free list of nodes.
 * New nodes go in front of head; the callers step head on to append. The instances only differ in the data after
 * next and prev, the helpers (src/game/Global/DList.c) take the node size.
 */
#ifndef SCRAP_DLIST_H
#define SCRAP_DLIST_H

#include "scrap/global.h"

#define DLIST(node) \
	struct { \
		node *head; \
		node *iter; \
		int count; \
		node *freeNodes; \
		int numFree; \
		int maxFree; \
	}

typedef struct cDListNode {
	struct cDListNode *next;	/* 0x0 */
	struct cDListNode *prev;	/* 0x4 */
} cDListNode;
typedef DLIST(cDListNode) cDList;

/* a node from the free list or new; the iterator is reset */
void *DList_NewNode(void *list, uint32_t size);
/* the node in front of head, as the new head */
void DList_LinkFirst(void *list, void *node);
/* the node in front of head, head kept: at the end of the ring */
void DList_LinkLast(void *list, void *node);
void DList_RemoveHead(void *list);
/* the node of the iterator; the iterator goes on to the next one (NULL at the end) */
void DList_RemoveIter(void *list);
void DList_Clear(void *list);
/* the free list deleted */
void DList_FreePool(void *list);

#endif
