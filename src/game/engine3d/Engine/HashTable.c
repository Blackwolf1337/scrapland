/*
 * Hash table of the engine3d, original source Engine/TablaHash.cpp (include/scrap/e3dhashtable.h).
 */
#include <string.h>

#include "scrap/e3dutil.h"
#include "scrap/e3dhashtable.h"

#define HASHTABLE_INTKEY 16

CHashTable *CHashTable_ctor(CHashTable *self, int num) {
	CHECK(num > 0);
	self->num = num;
	self->slots = New_Array(num, 4);
	self->lock = 0;
	memset(self->slots, 0, num * 4);
	return self;
}

void CHashTable_dtor(CHashTable *self) {
	CHECK(!self->lock);
	if (!self->slots) {
		return;
	}
	for (int i = 0; i < self->num; i++) {
		while (self->slots[i]) {
			CHashTableNode *n = self->slots[i];
			self->slots[i] = n->next;
			Mem_Free(n->key);
			operator_delete_sized(n, sizeof(*n));
		}
	}
	operator_delete_array(self->slots);
}

int CHashTable_AddInt(CHashTable *self, int key, void *value, int warn) {
	char text[HASHTABLE_INTKEY];

	Str_PrintfQuiet(text, sizeof(text), "%08X", key);
	return CHashTable_Add(self, text, value, warn);
}

int CHashTable_Add(CHashTable *self, const char *key, void *value, int warn) {
	CHECK(!self->lock);
	uint32_t slot = CHashTable_Slot(self, key);
	if (CHashTable_Find(self, key, slot)) {
		if (warn) {
			Err_Printf(0, "CTablaHash: S\xedmbolo ya definido:\n'%s'", key);
		}
		return -1;
	}
	CHashTable_Insert(self, key, slot, value);
	return 0;
}

CHashTableNode *CHashTable_Insert(CHashTable *self, const char *key, uint32_t slot, void *value) {
	CHECK(!self->lock);
	CHashTableNode *n = operator_new(sizeof(*n));
	n->next = self->slots[slot];
	n->key = String_Dup(key);
	n->value = value;
	self->slots[slot] = n;
	return n;
}

int CHashTable_GetInt(CHashTable *self, int key, void **out) {
	char text[HASHTABLE_INTKEY];

	Str_PrintfQuiet(text, sizeof(text), "%08X", key);
	return CHashTable_Get(self, text, out);
}

int CHashTable_Get(CHashTable *self, const char *key, void **out) {
	CHashTableNode *n = CHashTable_Find(self, key, CHashTable_Slot(self, key));

	if (!n) {
		return -1;
	}
	if (out) {
		*out = n->value;
	}
	return 0;
}

CHashTableNode *CHashTable_Find(CHashTable *self, const char *key, uint32_t slot) {
	for (CHashTableNode *n = self->slots[slot]; n; n = n->next) {
		if (!strcmp(n->key, key)) {
			return n;
		}
	}
	return NULL;
}

int CHashTable_FreeList(CHashTable *self, CHashTableNode ***elems) {
	CHECK(elems);
	self->lock--;
	operator_delete_array(*elems);
	*elems = NULL;
	return 0;
}

int CHashTable_List(CHashTable *self, CHashTableNode **out, int max) {
	int count = 0;

	for (int i = 0; i < self->num; i++) {
		for (CHashTableNode *n = self->slots[i]; n; n = n->next) {
			count++;
			if (out) {
				if (count > max) {
					return -1;
				}
				out[count - 1] = n;
			}
		}
	}
	return out ? 0 : count;
}

int CHashTable_GetList(CHashTable *self, CHashTableNode ***elems, int *num) {
	CHECK(elems && num);
	self->lock++;
	int count = CHashTable_List(self, NULL, 0);
	CHashTableNode **list = New_Array(count, 4);
	CHECK(!CHashTable_List(self, list, count));
	*elems = list;
	*num = count;
	return 0;
}

uint32_t CHashTable_Slot(CHashTable *self, const char *key) {
	uint32_t h = 0;

	CHECK(key);
	for (const byte *p = (const byte *)key; *p; p++) {
		h = Hash_Step(h, *p);
	}
	return h % (uint32_t)self->num;
}
