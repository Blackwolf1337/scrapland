/*
 * CHashTable of the engine3d (original source Engine/TablaHash.cpp): chained hash of case-sensitive string keys that
 * are copied (String_Dup), with a pointer value. Integer keys are the "%08X" text of the number. While a list from
 * CHashTable_GetList is out the table is locked against changes.
 */
#ifndef SCRAP_E3DHASHTABLE_H
#define SCRAP_E3DHASHTABLE_H

#include "scrap/game.h"

typedef struct CHashTableNode {
	struct CHashTableNode *next;
	char *key;
	void *value;
} CHashTableNode;

typedef struct CHashTable {
	int num;			/* slots */
	CHashTableNode **slots;
	int lock;			/* lists out */
} CHashTable;

CHashTable *CHashTable_ctor(CHashTable *self, int num);
void CHashTable_dtor(CHashTable *self);
/* 0, -1 if key is there (with a message if warn) */
int CHashTable_AddInt(CHashTable *self, int key, void *value, int warn);
/* 0 and the value in *out (if out), -1 if key is not there */
int CHashTable_GetInt(CHashTable *self, int key, void **out);
/* the end of a CHashTable_GetList */
int CHashTable_FreeList(CHashTable *self, CHashTableNode ***elems);
/* all nodes in a new array; locks the table until CHashTable_FreeList */
int CHashTable_GetList(CHashTable *self, CHashTableNode ***elems, int *num);

#endif
