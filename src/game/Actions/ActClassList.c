/*
 * Actions/ActClassList.cpp of the game: the action classes by name (g_pActClassList). See include/scrap/actions.h.
 */
#include "scrap/actions.h"
#include "scrap/console.h"
#include "scrap/hashtable.h"

#define CACTCLASSLIST_VFTABLE (&cActClassList_vftable)
static const cActClassListVtbl cActClassList_vftable;

#define ACTCLASSLIST_SLOTS 457

/* Actions/ActClassList.cpp (src/game/Actions/ActClassList.c) */
static cActClassList *cActClassList_ctor(cActClassList *self) {
	self->vftable = CACTCLASSLIST_VFTABLE;
	HashTable_ctor(&self->classes, ACTCLASSLIST_SLOTS, 1);
	return self;
}

static void cActClassList_dtor(cActClassList *self) {
	HashTable *t = &self->classes;

	self->vftable = CACTCLASSLIST_VFTABLE;
	for (;;) {
		int k = 0;
		if ((int)t->numSlots < 1) {
			break;
		}
		while (!t->slots[k]) {
			if (++k >= (int)t->numSlots) {
				goto done;
			}
		}
		cPtrItem *it = (cPtrItem *)t->slots[k];
		cActClass *cls = it->value;
		byte deleted = PtrTable_Delete(t, it->key);
		CHECK(deleted);
		CHECK(cls != NULL);
		if (cls) {
			cls->vftable->Delete(cls, 1);
		}
	}
done:
	PtrTable_Clear(t);
}

static cActClassList *cActClassList_Delete(cActClassList *self, byte flags) {
	cActClassList_dtor(self);
	if (flags & 1) {
		operator_delete_sized(self, sizeof(*self));
	}
	return self;
}

cActClass *cActClassList_Create(cActClassList *list, const char *name) {
	HashTable *t = &list->classes;
	cActClass *cls = operator_new(sizeof(cActClass));

	if (cls) {
		cls = cActClass_ctor(cls, name);
	}
	const char *key = cls->name;
	CHECK(!t->locked);
	uint32_t slot = HashTable_Slot(t, key);
	byte added = 0;
	if (PtrTable_Find(t, key, slot)) {
		Con_Warnf("CHashTable3: Symbol already defined: '%s'\n", key);
	} else {
		PtrTable_Insert(t, key, slot, (void *const *)&cls);
		added = 1;
	}
	CHECK(added);
	return cls;
}

cActClass *cActClassList_Get(cActClassList *list, const char *name) {
	return PtrTable_Value(&list->classes, name);
}

byte cActClassList_Del(cActClassList *list, const char *name) {
	HashTable *t = &list->classes;
	cPtrItem *it = PtrTable_Find(t, name, HashTable_Slot(t, name));

	if (it) {
		cActClass *cls = it->value;
		if (cls->users == 0) {
			PtrTable_Delete(t, name);
			cls->vftable->Delete(cls, 1);
			return 1;
		}
	}
	return 0;
}

void ActClassList_Done(void) {
	cActClassList *list = g_pActClassList;

	if (!list) {
		Con_Warnf("WARNING at DoneActClassList(): ActClassList can\xb4t be deleted because doesn\xb4t exists.\n");
		return;
	}
	list->vftable->Delete(list, 1);
	g_pActClassList = NULL;
	g_pAuxActClass = NULL;
	g_pAuxAction = NULL;
}

void ActClassList_Init(void) {
	if (g_pActClassList) {
		Con_Warnf("WARNING at InitActClassList(): ActClassList can\xb4t be created because already exists.\n");
		return;
	}
	cActClassList *list = operator_new(sizeof(cActClassList));
	g_pActClassList = list ? cActClassList_ctor(list) : NULL;
	g_pAuxActClass = NULL;
	g_pAuxAction = NULL;
}

/* the vtable of cActClassList */
static const cActClassListVtbl cActClassList_vftable = {
	.Delete = cActClassList_Delete,
};
