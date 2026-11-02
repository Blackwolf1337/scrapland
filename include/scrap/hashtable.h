/*
 * Global/HashTable.h: a template the engine instantiates per item type. Items come from the shared "HashItem"
 * pool and hold the value, the key and the next item of the slot. The shared parts are in
 * src/game/Global/HashTable.c; an instance with another item layout has its own functions next to its users.
 */
#ifndef SCRAP_HASHTABLE_H
#define SCRAP_HASHTABLE_H

#include "scrap/game.h"
#include "scrap/pymempool.h"

extern cMemPool *g_HashItemPool;
extern int g_HashItemRefs;
extern uint32_t g_HashItemValues[32];	/* [MEMPOOL_CLASSES] */

HashTable *HashTable_ctor(HashTable *self, int numSlots, byte caseSensitive);
/* the slot of key: a 4 bit shift hash folded to 28 bits, case-insensitive unless caseSensitive (global.h) */
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

cPtrItem *PtrTable_Insert(HashTable *self, const char *key, uint32_t slot, void *const *value);
byte PtrTable_Add(HashTable *self, const char *key, void *const *value, byte warn);
cPtrItem *PtrTable_Unlink(HashTable *self, const char *key, uint32_t slot);
byte PtrTable_Delete(HashTable *self, const char *key);
void PtrTable_Clear(HashTable *self);
/* the value of key, NULL if it is not there */
void *PtrTable_Value(HashTable *self, const char *key);

/*
 * The older HashTable.h, with the key copied into a cString. HASHTABLE_STR(P, ITEM) defines P_Find, P_Insert,
 * P_Add, P_Get, P_Lookup, P_Set, P_Unlink, P_Delete, P_Clear and P_dtor for an ITEM { value; char *key; next; };
 * the instances only differ in the value.
 */
#define HASHTABLE_STR(P, ITEM) \
	ITEM *P##_Find(HashTable *self, const char *key, uint32_t slot) { \
		for (ITEM *it = (ITEM *)self->slots[slot]; it; it = it->next) { \
			if (HashTable_KeyEq(self, it->key, key)) { \
				return it; \
			} \
		} \
		return NULL; \
	} \
	ITEM *P##_Insert(HashTable *self, const char *key, uint32_t slot, const typeof(((ITEM *)0)->value) *v) { \
		CHECK(!self->locked); \
		ITEM *it = HashItem_Alloc(sizeof(*it)); \
		if (it) { \
			cString_ctor(&it->key); \
			it->value = *v; \
			it->next = NULL; \
		} \
		it->next = (ITEM *)self->slots[slot]; \
		cString_Set(&it->key, key); \
		self->slots[slot] = (HashNode *)it; \
		return it; \
	} \
	byte P##_Add(HashTable *self, const char *key, const typeof(((ITEM *)0)->value) *v, byte warn) { \
		CHECK(!self->locked); \
		uint32_t slot = HashTable_Slot(self, key); \
		if (P##_Find(self, key, slot)) { \
			if (warn) { \
				Con_Warnf("CHashTable: Symbol already defined: '%s'\n", key); \
			} \
			return 0; \
		} \
		P##_Insert(self, key, slot, v); \
		return 1; \
	} \
	byte P##_Get(HashTable *self, const char *key, typeof(((ITEM *)0)->value) *out) { \
		ITEM *it = P##_Find(self, key, HashTable_Slot(self, key)); \
		if (!it) { \
			return 0; \
		} \
		*out = it->value; \
		return 1; \
	} \
	void P##_Lookup(HashTable *self, const char *key, typeof(((ITEM *)0)->value) *out) { \
		ITEM *it = P##_Find(self, key, HashTable_Slot(self, key)); \
		if (it) { \
			*out = it->value; \
		} \
	} \
	byte P##_Set(HashTable *self, const char *key, const typeof(((ITEM *)0)->value) *v) { \
		CHECK(!self->locked); \
		ITEM *it = P##_Find(self, key, HashTable_Slot(self, key)); \
		if (!it) { \
			return 0; \
		} \
		it->value = *v; \
		return 1; \
	} \
	ITEM *P##_Unlink(HashTable *self, const char *key, uint32_t slot) { \
		ITEM **link = (ITEM **)&self->slots[slot]; \
		CHECK(!self->locked); \
		for (; *link; link = &(*link)->next) { \
			if (HashTable_KeyEq(self, (*link)->key, key)) { \
				ITEM *it = *link; \
				*link = it->next; \
				it->next = NULL; \
				return it; \
			} \
		} \
		return NULL; \
	} \
	byte P##_Delete(HashTable *self, const char *key) { \
		CHECK(!self->locked); \
		ITEM *it = P##_Unlink(self, key, HashTable_Slot(self, key)); \
		if (!it) { \
			return 0; \
		} \
		cString_dtor(&it->key); \
		HashItem_Free(it); \
		return 1; \
	} \
	void P##_Clear(HashTable *self) { \
		CHECK(!self->locked); \
		if (!self->slots) { \
			return; \
		} \
		for (int i = 0; i < (int)self->numSlots; i++) { \
			while (self->slots[i]) { \
				ITEM *it = (ITEM *)self->slots[i]; \
				self->slots[i] = (HashNode *)it->next; \
				cString_dtor(&it->key); \
				HashItem_Free(it); \
			} \
		} \
		operator_delete(self->slots); \
		self->numSlots = 0; \
		self->slots = NULL; \
	} \
	void P##_dtor(HashTable *self) { \
		P##_Clear(self); \
	}

/* the instance of World/ with a 12 byte value (a vec3_t: the lairs of Logic/Scrapyard.c) */
typedef struct cValue12 {
	uint32_t v[3];
} cValue12;

typedef struct cWorldStrItem {
	cValue12 value;			/* 0x00 */
	char *key;			/* 0x0c cString */
	struct cWorldStrItem *next;	/* 0x10 */
} cWorldStrItem;
SD_SIZE(cWorldStrItem, 0x14);

cWorldStrItem *WorldStrTable_Find(HashTable *self, const char *key, uint32_t slot);
cWorldStrItem *WorldStrTable_Insert(HashTable *self, const char *key, uint32_t slot, const cValue12 *v);
byte WorldStrTable_Set(HashTable *self, const char *key, const cValue12 *v);
byte WorldStrTable_Delete(HashTable *self, const char *key);
void WorldStrTable_Clear(HashTable *self);

#endif
