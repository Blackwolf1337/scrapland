/*
 * Hash table of the engine3d, original source Engine/TablaHash.cpp (include/scrap/tablahash.h).
 */
#include <string.h>

#include "scrap/e3dutil.h"
#include "scrap/tablahash.h"

#define TABLAHASH_INTKEY 16

CTablaHash *__thiscall CTablaHash_ctor(CTablaHash *self, int num) {
	CHECK(num > 0);
	self->num = num;
	self->slots = New_Array(num, 4);
	self->lock = 0;
	memset(self->slots, 0, num * 4);
	return self;
}

void __fastcall CTablaHash_dtor(CTablaHash *self) {
	CHECK(!self->lock);
	if (!self->slots) {
		return;
	}
	for (int i = 0; i < self->num; i++) {
		while (self->slots[i]) {
			CTablaHashNodo *n = self->slots[i];
			self->slots[i] = n->next;
			Mem_Free(n->key);
			operator_delete_sized(n, sizeof(*n));
		}
	}
	operator_delete_array(self->slots);
}

int __thiscall CTablaHash_AddInt(CTablaHash *self, int key, void *value, int warn) {
	char text[TABLAHASH_INTKEY];

	Str_PrintfQuiet(text, sizeof(text), "%08X", key);
	return CTablaHash_Add(self, text, value, warn);
}

int __thiscall CTablaHash_Add(CTablaHash *self, const char *key, void *value, int warn) {
	CHECK(!self->lock);
	uint32_t slot = CTablaHash_Slot(self, key);
	if (CTablaHash_Find(self, key, slot)) {
		if (warn) {
			Err_Printf(0, "CTablaHash: S\xedmbolo ya definido:\n'%s'", key);
		}
		return -1;
	}
	CTablaHash_Insert(self, key, slot, value);
	return 0;
}

CTablaHashNodo *__thiscall CTablaHash_Insert(CTablaHash *self, const char *key, uint32_t slot, void *value) {
	CHECK(!self->lock);
	CTablaHashNodo *n = operator_new(sizeof(*n));
	n->next = self->slots[slot];
	n->key = Cad_Dup(key);
	n->value = value;
	self->slots[slot] = n;
	return n;
}

int __thiscall CTablaHash_GetInt(CTablaHash *self, int key, void **out) {
	char text[TABLAHASH_INTKEY];

	Str_PrintfQuiet(text, sizeof(text), "%08X", key);
	return CTablaHash_Get(self, text, out);
}

int __thiscall CTablaHash_Get(CTablaHash *self, const char *key, void **out) {
	CTablaHashNodo *n = CTablaHash_Find(self, key, CTablaHash_Slot(self, key));

	if (!n) {
		return -1;
	}
	if (out) {
		*out = n->value;
	}
	return 0;
}

CTablaHashNodo *__thiscall CTablaHash_Find(CTablaHash *self, const char *key, uint32_t slot) {
	for (CTablaHashNodo *n = self->slots[slot]; n; n = n->next) {
		if (!strcmp(n->key, key)) {
			return n;
		}
	}
	return NULL;
}

int __thiscall CTablaHash_FreeList(CTablaHash *self, CTablaHashNodo ***elems) {
	CHECK(elems);
	self->lock--;
	operator_delete_array(*elems);
	*elems = NULL;
	return 0;
}

int __thiscall CTablaHash_List(CTablaHash *self, CTablaHashNodo **out, int max) {
	int count = 0;

	for (int i = 0; i < self->num; i++) {
		for (CTablaHashNodo *n = self->slots[i]; n; n = n->next) {
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

int __thiscall CTablaHash_GetList(CTablaHash *self, CTablaHashNodo ***elems, int *num) {
	CHECK(elems && num);
	self->lock++;
	int count = CTablaHash_List(self, NULL, 0);
	CTablaHashNodo **list = New_Array(count, 4);
	CHECK(!CTablaHash_List(self, list, count));
	*elems = list;
	*num = count;
	return 0;
}

uint32_t __thiscall CTablaHash_Slot(CTablaHash *self, const char *key) {
	uint32_t h = 0;

	CHECK(key);
	for (const byte *p = (const byte *)key; *p; p++) {
		h = Hash_Step(h, *p);
	}
	return h % (uint32_t)self->num;
}
