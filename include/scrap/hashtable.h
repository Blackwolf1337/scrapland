/*
 * Global/HashTable.h: a template the engine instantiates per item type. Items come from the shared "HashItem"
 * pool and hold the value, the key and the next item of the slot. The shared parts are in
 * src/game/Global/HashTable.c; an instance with another item layout has its own functions next to its users.
 */
#ifndef SCRAP_HASHTABLE_H
#define SCRAP_HASHTABLE_H

#include "scrap/game.h"
#include "scrap/pymempool.h"

#define g_HashItemPool GAME_DATA(cMemPool *, 0x44b600)
#define g_HashItemRefs GAME_DATA(int, 0x44b604)
#define g_HashItemValues (&GAME_DATA(uint32_t, 0x4289a0))

HashTable *__thiscall HashTable_ctor(HashTable *self, int numSlots, byte caseSensitive);
/* the slot of key: a 4 bit shift hash folded to 28 bits, case-insensitive unless caseSensitive */
uint32_t __thiscall HashTable_Slot(HashTable *self, const char *key);
/* equal keys in the case mode of the table */
int HashTable_KeyEq(HashTable *self, const char *a, const char *b);
/* an item from the pool, which is made on first use */
void *HashItem_Alloc(uint32_t size);
/* the pool goes when its last item does */
void HashItem_Free(void *p);

/* the instance with a pointer value and a key that is not copied (cGVar, cSaveVar) */
typedef struct cPtrItem {
	void *value;			/* 0x0 */
	const char *key;		/* 0x4 */
	struct cPtrItem *next;		/* 0x8 */
} cPtrItem;
SD_SIZE(cPtrItem, 0xc);

cPtrItem *__thiscall PtrTable_Find(HashTable *self, const char *key, uint32_t slot);
cPtrItem *__thiscall PtrTable_Insert(HashTable *self, const char *key, uint32_t slot, void *const *value);
byte __thiscall PtrTable_Add(HashTable *self, const char *key, void *const *value, byte warn);
byte __thiscall PtrTable_Get(HashTable *self, const char *key, void **out);
cPtrItem *__thiscall PtrTable_Unlink(HashTable *self, const char *key, uint32_t slot);
byte __thiscall PtrTable_Delete(HashTable *self, const char *key);
void __fastcall PtrTable_Clear(HashTable *self);
/* the value of key, NULL if it is not there */
void *PtrTable_Value(HashTable *self, const char *key);

#endif
