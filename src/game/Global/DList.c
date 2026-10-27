/*
 * The node handling of the Global/DList.h instances (include/scrap/dlist.h).
 */
#include "scrap/dlist.h"

void *DList_NewNode(void *list, uint32_t size) {
	cDList *l = list;
	cDListNode *node = l->freeNodes;

	l->iter = NULL;
	if (!node) {
		return operator_new(size);
	}
	l->numFree--;
	l->freeNodes = node->next;
	return node;
}

void DList_LinkFirst(void *list, void *node) {
	cDList *l = list;
	cDListNode *n = node;

	if (!l->head) {
		n->next = n;
		n->prev = n;
	} else {
		n->prev = l->head->prev;
		n->next = l->head;
		l->head->prev->next = n;
		l->head->prev = n;
	}
	l->count++;
	l->head = n;
}

void DList_LinkLast(void *list, void *node) {
	cDList *l = list;
	cDListNode *n = node;

	if (!l->head) {
		n->next = n;
		n->prev = n;
		l->count++;
		l->head = n;
		return;
	}
	n->prev = l->head->prev;
	n->next = l->head;
	l->head->prev->next = n;
	l->head->prev = n;
	l->count++;
}

void DList_Release(void *list, void *node, uint32_t size) {
	cDList *l = list;
	cDListNode *n = node;

	if (l->numFree < l->maxFree) {
		n->next = l->freeNodes;
		l->numFree++;
		l->freeNodes = n;
	} else {
		operator_delete_sized(n, size);
	}
}

void DList_RemoveHead(void *list, uint32_t size) {
	cDList *l = list;
	cDListNode *p = l->head;

	l->iter = NULL;
	if (!p) {
		return;
	}
	if (p == p->next) {
		l->head = NULL;
	} else {
		l->head = p->next;
		p->prev->next = p->next;
		p->next->prev = p->prev;
	}
	DList_Release(l, p, size);
	l->count--;
}

void DList_RemoveIter(void *list, uint32_t size) {
	cDList *l = list;
	cDListNode *cur = l->iter;

	if (!cur) {
		return;
	}
	if (l->head == cur) {
		DList_RemoveHead(l, size);
		l->iter = l->head;
		return;
	}
	cDListNode *next = cur->next;
	cur->prev->next = next;
	cur->next->prev = cur->prev;
	DList_Release(l, cur, size);
	if (next == l->head) {
		next = NULL;
	}
	l->count--;
	l->iter = next;
}

void DList_Clear(void *list, uint32_t size) {
	while (((cDList *)list)->head) {
		DList_RemoveHead(list, size);
	}
}

void DList_FreePool(void *list, uint32_t size) {
	cDList *l = list;

	while (l->freeNodes) {
		cDListNode *n = l->freeNodes;
		l->freeNodes = n->next;
		operator_delete_sized(n, size);
	}
	l->numFree = 0;
}
