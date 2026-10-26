/*
 * CTablaHash of the engine3d (original source Engine/TablaHash.cpp): chained hash of case-sensitive string keys that
 * are copied (Cad_Dup), with a pointer value. Integer keys are the "%08X" text of the number. While a list from
 * CTablaHash_GetList is out the table is locked against changes.
 */
#ifndef SCRAP_TABLAHASH_H
#define SCRAP_TABLAHASH_H

#include "scrap/game.h"

typedef struct CTablaHashNodo {
	struct CTablaHashNodo *next;	/* 0x0 */
	char *key;			/* 0x4 */
	void *value;			/* 0x8 */
} CTablaHashNodo;
SD_SIZE(CTablaHashNodo, 0xc);

typedef struct CTablaHash {
	int num;			/* 0x0 slots */
	CTablaHashNodo **slots;		/* 0x4 */
	int lock;			/* 0x8 lists out */
} CTablaHash;
SD_SIZE(CTablaHash, 0xc);

CTablaHash *__thiscall CTablaHash_ctor(CTablaHash *self, int num);
void __fastcall CTablaHash_dtor(CTablaHash *self);
/* 0, -1 if key is there (with a message if warn) */
int __thiscall CTablaHash_AddInt(CTablaHash *self, int key, void *value, int warn);
int __thiscall CTablaHash_Add(CTablaHash *self, const char *key, void *value, int warn);
CTablaHashNodo *__thiscall CTablaHash_Insert(CTablaHash *self, const char *key, uint32_t slot, void *value);
/* 0 and the value in *out (if out), -1 if key is not there */
int __thiscall CTablaHash_GetInt(CTablaHash *self, int key, void **out);
int __thiscall CTablaHash_Get(CTablaHash *self, const char *key, void **out);
CTablaHashNodo *__thiscall CTablaHash_Find(CTablaHash *self, const char *key, uint32_t slot);
/* the end of a CTablaHash_GetList */
int __thiscall CTablaHash_FreeList(CTablaHash *self, CTablaHashNodo ***elems);
/* the nodes into out (up to max, else -1) and 0, or with out NULL their number */
int __thiscall CTablaHash_List(CTablaHash *self, CTablaHashNodo **out, int max);
/* all nodes in a new array; locks the table until CTablaHash_FreeList */
int __thiscall CTablaHash_GetList(CTablaHash *self, CTablaHashNodo ***elems, int *num);
uint32_t __thiscall CTablaHash_Slot(CTablaHash *self, const char *key);

#endif
