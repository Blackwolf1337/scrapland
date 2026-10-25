/*
 * Global/Nto1Links.h: many links to one object. The object holds a head node, every link a node in the ring of the
 * head. A head has data NULL and target the object; a link has data set (itself or its owner) and target the
 * object while it is linked. When the object goes, the head unlinks all nodes and their target becomes NULL.
 */
#ifndef SCRAP_NTO1LINKS_H
#define SCRAP_NTO1LINKS_H

#include "scrap/global.h"

typedef struct Nto1Node {
	struct Nto1Node *next;		/* 0x0 */
	struct Nto1Node *prev;		/* 0x4 */
	void *target;			/* 0x8 the linked object, NULL when unlinked */
	void *data;			/* 0xc NULL for the head */
} Nto1Node;
SD_SIZE(Nto1Node, 0x10);

static inline void Nto1Node_Init(Nto1Node *node) {
	node->next = node;
	node->prev = node;
	node->target = NULL;
	node->data = NULL;
}

/* takes the node out of its ring; the original checks Node (data) in some instances only (line 0x4e) */
static inline void Nto1Node_Unlink(Nto1Node *node) {
	node->next->prev = node->prev;
	node->prev->next = node->next;
	node->target = NULL;
	node->prev = node;
	node->next = node;
}

/* unlinks every node of the head (rva 0xd40f0, ecx = head) */
void __fastcall Nto1Head_UnlinkAll(Nto1Node *head);

/* destructor of a node, inlined everywhere */
static inline void Nto1Node_Done(Nto1Node *node) {
	if (!node->data) {
		Nto1Head_UnlinkAll(node);
	} else {
		Nto1Node_Unlink(node);
	}
}

#endif
