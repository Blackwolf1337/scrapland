/*
 * CHashTable of the engine3d (original source Engine/TablaHash.cpp): chained hash of case-sensitive string keys that
 * are copied (String_Dup), with a pointer value. Integer keys are the "%08X" text of the number. While a list from
 * CHashTable_GetList is out the table is locked against changes.
 */
#ifndef SCRAP_E3DHASHTABLE_H
#define SCRAP_E3DHASHTABLE_H

#include "scrap/game.h"

typedef struct CHashTableNode {
	struct CHashTableNode *next;	/* 0x0 */
	char *key;			/* 0x4 */
	void *value;			/* 0x8 */
} CHashTableNode;
SD_SIZE(CHashTableNode, 0xc);

typedef struct CHashTable {
	int num;			/* 0x0 slots */
	CHashTableNode **slots;		/* 0x4 */
	int lock;			/* 0x8 lists out */
} CHashTable;
SD_SIZE(CHashTable, 0xc);

CHashTable *__thiscall CHashTable_ctor(CHashTable *self, int num);
void __fastcall CHashTable_dtor(CHashTable *self);
/* 0, -1 if key is there (with a message if warn) */
int __thiscall CHashTable_AddInt(CHashTable *self, int key, void *value, int warn);
int __thiscall CHashTable_Add(CHashTable *self, const char *key, void *value, int warn);
CHashTableNode *__thiscall CHashTable_Insert(CHashTable *self, const char *key, uint32_t slot, void *value);
/* 0 and the value in *out (if out), -1 if key is not there */
int __thiscall CHashTable_GetInt(CHashTable *self, int key, void **out);
int __thiscall CHashTable_Get(CHashTable *self, const char *key, void **out);
CHashTableNode *__thiscall CHashTable_Find(CHashTable *self, const char *key, uint32_t slot);
/* the end of a CHashTable_GetList */
int __thiscall CHashTable_FreeList(CHashTable *self, CHashTableNode ***elems);
/* the nodes into out (up to max, else -1) and 0, or with out NULL their number */
int __thiscall CHashTable_List(CHashTable *self, CHashTableNode **out, int max);
/* all nodes in a new array; locks the table until CHashTable_FreeList */
int __thiscall CHashTable_GetList(CHashTable *self, CHashTableNode ***elems, int *num);
uint32_t __thiscall CHashTable_Slot(CHashTable *self, const char *key);

#endif
