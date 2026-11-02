/*
 * The shared parts of the Global/HashTable.h instances (include/scrap/hashtable.h), and the instance with a
 * pointer value and a plain key.
 */
#include <string.h>

#include "scrap/console.h"
#include "scrap/hashtable.h"

HashTable *HashTable_ctor(HashTable *self, int numSlots, byte caseSensitive) {
	CHECK(numSlots > 0);
	self->numSlots = numSlots;
	self->slots = New_Array(numSlots, sizeof(HashNode *));
	memset(self->slots, 0, numSlots * sizeof(HashNode *));
	self->caseSensitive = caseSensitive;
	self->locked = 0;
	return self;
}

uint32_t HashTable_Slot(HashTable *self, const char *key) {
	uint32_t h = 0;

	CHECK(key);
	CHECK(self->numSlots);
	for (const byte *p = (const byte *)key; *p; p++) {
		byte c = *p;
		if (!self->caseSensitive && (byte)(c - 'A') < 26) {
			c += 0x20;
		}
		h = Hash_Step(h, c);
	}
	return h % self->numSlots;
}

int HashTable_KeyEq(HashTable *self, const char *a, const char *b) {
	return self->caseSensitive ? strcmp(a, b) == 0 : crt_stricmp(a, b) == 0;
}

void *HashItem_Alloc(uint32_t size) {
	if (!g_HashItemPool) {
		cMemPool *mem = operator_new(sizeof(*mem));
		g_HashItemPool = mem ? cMemPool_ctor(mem, "HashItem", g_HashItemValues) : NULL;
	}
	g_HashItemRefs++;
	return cMemPool_Alloc(g_HashItemPool, size);
}

void HashItem_Free(void *p) {
	g_HashItemRefs--;
	cMemPool_Free(g_HashItemPool, p);
	cMemPool *pool = g_HashItemPool;
	if (g_HashItemRefs < 1) {
		if (pool) {
			cMemPool_dtor(pool);
			operator_delete_sized(pool, sizeof(*pool));
		}
		g_HashItemPool = NULL;
	}
}

cPtrItem *PtrTable_Find(HashTable *self, const char *key, uint32_t slot) {
	for (cPtrItem *it = (cPtrItem *)self->slots[slot]; it; it = it->next) {
		if (HashTable_KeyEq(self, it->key, key)) {
			return it;
		}
	}
	return NULL;
}

cPtrItem *PtrTable_Insert(HashTable *self, const char *key, uint32_t slot, void *const *value) {
	CHECK(!self->locked);
	cPtrItem *it = HashItem_Alloc(sizeof(*it));
	if (it) {
		it->value = *value;
		it->next = NULL;
		it->key = NULL;
	}
	it->next = (cPtrItem *)self->slots[slot];
	it->key = key;
	self->slots[slot] = (HashNode *)it;
	return it;
}

byte PtrTable_Add(HashTable *self, const char *key, void *const *value, byte warn) {
	CHECK(!self->locked);
	uint32_t slot = HashTable_Slot(self, key);
	if (PtrTable_Find(self, key, slot)) {
		if (warn) {
			Con_Warnf("CHashTable3: Symbol already defined: '%s'\n", key);
		}
		return 0;
	}
	PtrTable_Insert(self, key, slot, value);
	return 1;
}

void *PtrTable_Value(HashTable *self, const char *key) {
	cPtrItem *it = PtrTable_Find(self, key, HashTable_Slot(self, key));
	return it ? it->value : NULL;
}

/* takes the item of key out of its slot, NULL if there is none */
cPtrItem *PtrTable_Unlink(HashTable *self, const char *key, uint32_t slot) {
	cPtrItem **link = (cPtrItem **)&self->slots[slot];
	CHECK(!self->locked);
	for (; *link; link = &(*link)->next) {
		if (HashTable_KeyEq(self, (*link)->key, key)) {
			cPtrItem *it = *link;
			*link = it->next;
			it->next = NULL;
			return it;
		}
	}
	return NULL;
}

byte PtrTable_Delete(HashTable *self, const char *key) {
	CHECK(!self->locked);
	cPtrItem *it = PtrTable_Unlink(self, key, HashTable_Slot(self, key));
	if (!it) {
		return 0;
	}
	HashItem_Free(it);
	return 1;
}

void PtrTable_Clear(HashTable *self) {
	CHECK(!self->locked);
	if (!self->slots) {
		return;
	}
	for (int i = 0; i < (int)self->numSlots; i++) {
		while (self->slots[i]) {
			cPtrItem *it = (cPtrItem *)self->slots[i];
			self->slots[i] = (HashNode *)it->next;
			HashItem_Free(it);
		}
	}
	operator_delete_array(self->slots);
	self->numSlots = 0;
	self->slots = NULL;
}
