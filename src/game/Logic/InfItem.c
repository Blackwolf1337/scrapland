/*
 * Logic/InfItem.cpp of the game: cInfItem (include/scrap/logic.h), the item list of the world (Scrap.AddItem, the
 * cItemList of game.h). The items are copies of cItemDesc in DLists (include/scrap/dlist.h) of string nodes: the
 * life items and the ammo items of the kinds 0, 1 and 2. The walks keep the iterator of the list. The two
 * template instances of the node functions are the same code.
 */
#include <string.h>

#include "scrap/cstring.h"
#include "scrap/logic.h"

cInfItem *__fastcall cInfItem_ctor(cInfItem *self) {
	self->vftable = Game_Ptr(CINFITEM_VFTABLE);
	memset(&self->life, 0, sizeof(*self) - 4);
	return self;
}

/* a node of the free list or a new one with its string */
cInfItemNode *__fastcall InfItemList_NewNode(cInfItemList *list) {
	cInfItemNode *n = list->freeNodes;

	if (n) {
		list->numFree--;
		list->freeNodes = n->next;
		return n;
	}
	n = operator_new(sizeof(*n));
	if (!n) {
		return NULL;
	}
	cString_ctor(&n->item.name);
	return n;
}

/* to the free list while it has room, else the string and the node go */
static void InfItemList_Release(cInfItemList *list, cInfItemNode *n) {
	if (list->numFree < list->maxFree) {
		n->next = list->freeNodes;
		list->numFree++;
		list->freeNodes = n;
		return;
	}
	cString_dtor(&n->item.name);
	operator_delete_sized(n, sizeof(*n));
}

void __fastcall InfItemList_Clear(cInfItemList *list) {
	while (list->head) {
		cInfItemNode *p = list->head;
		list->iter = NULL;
		if (p == p->next) {
			InfItemList_Release(list, p);
			list->head = NULL;
		} else {
			list->head = p->next;
			p->prev->next = p->next;
			p->next->prev = p->prev;
			InfItemList_Release(list, p);
		}
		list->count--;
	}
}

void __fastcall InfItemList_FreePool(cInfItemList *list) {
	cInfItemNode *p;

	while ((p = list->freeNodes) != NULL) {
		list->freeNodes = p->next;
		cString_dtor(&p->item.name);
		operator_delete_sized(p, sizeof(*p));
	}
	list->numFree = 0;
}

/* a copy of the item as the new head */
void __thiscall InfItemList_Add(cInfItemList *list, cItemDesc *item) {
	list->iter = NULL;
	cInfItemNode *n = InfItemList_NewNode(list);
	cString_Assign(&n->item.name, &item->name);
	n->item.value = item->value;
	DList_LinkFirst(list, n);
}

void __fastcall cInfItem_dtor(cInfItem *self) {
	self->vftable = Game_Ptr(CINFITEM_VFTABLE);
	InfItemList_Clear(&self->ammo1);
	InfItemList_FreePool(&self->ammo1);
	InfItemList_Clear(&self->ammo2);
	InfItemList_FreePool(&self->ammo2);
	InfItemList_Clear(&self->ammo0);
	InfItemList_FreePool(&self->ammo0);
	InfItemList_Clear(&self->life);
	InfItemList_FreePool(&self->life);
}

cInfItem *__thiscall cInfItem_Delete(cInfItem *self, byte flags) {
	cInfItem_dtor(self);
	if (flags & 1) {
		operator_delete_sized(self, sizeof(*self));
	}
	return self;
}

/* the list of an ammo kind; any other kind is taken as the list itself, like the original */
static inline cInfItemList *InfItem_AmmoList(cInfItem *self, int kind) {
	switch (kind) {
	case 0:
		return &self->ammo0;
	case 1:
		return &self->ammo1;
	case 2:
		return &self->ammo2;
	}
	return (cInfItemList *)kind;
}

static cItemDesc *InfItemList_First(cInfItemList *list, void **iter) {
	cInfItemNode *n = list->head;

	*iter = n;
	return n ? &n->item : NULL;
}

static cItemDesc *InfItemList_Next(cInfItemList *list, void **iter) {
	cInfItemNode *n = ((cInfItemNode *)*iter)->next;

	*iter = n;
	return n != list->head ? &n->item : NULL;
}

/* vf01 */
void __thiscall cInfItem_AddLife(cInfItem *self, cItemDesc *item) {
	cInfItemNode *save = self->life.iter;

	InfItemList_Add(&self->life, item);
	self->life.iter = save;
}

/* vf02 */
cItemDesc *__thiscall cInfItem_FirstLife(cInfItem *self, void **iter) {
	return InfItemList_First(&self->life, iter);
}

/* vf03 */
cItemDesc *__thiscall cInfItem_NextLife(cInfItem *self, void **iter) {
	return InfItemList_Next(&self->life, iter);
}

/* vf04 */
void __thiscall cInfItem_AddAmmo(cInfItem *self, int kind, cItemDesc *item) {
	cInfItemList *l;

	switch (kind) {
	case 0:
		l = &self->ammo0;
		break;
	case 1:
		l = &self->ammo1;
		break;
	case 2:
		l = &self->ammo2;
		break;
	default:
		CHECK(0);
		l = (cInfItemList *)kind;
		break;
	}
	CHECK(l);
	cInfItemNode *save = l->iter;
	InfItemList_Add(l, item);
	l->iter = save;
}

/* vf05 */
cItemDesc *__thiscall cInfItem_FirstAmmo(cInfItem *self, int kind, void **iter) {
	return InfItemList_First(InfItem_AmmoList(self, kind), iter);
}

/* vf06 */
cItemDesc *__thiscall cInfItem_NextAmmo(cInfItem *self, int kind, void **iter) {
	return InfItemList_Next(InfItem_AmmoList(self, kind), iter);
}
