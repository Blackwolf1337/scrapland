/*
 * Engine/EventoAnim.cpp of the engine3d: the events of the animations (CAnimEvent, include/scrap/controlanim.h), a
 * list sorted by the time.
 */
#include "scrap/controlanim.h"

#define ANIMEVENT_INSERTED 1		/* flags: in a list */

CAnimEvent *CAnimEvent_ctor(CAnimEvent *self, float t, int id, intptr_t arg) {
	self->id = id;
	self->arg = arg;
	self->t = t;
	self->next = NULL;
	self->flags = 0;
	return self;
}

void CAnimEvent_dtor(CAnimEvent *self) {
	CAnimEvent *p = self->next;
	if (p) {
		CAnimEvent_dtor(p);
		operator_delete(p);
		self->next = NULL;
	}
}

/* into the list sorted by t, then id; CHECK it is in none yet */
static void CAnimEvent_Insert(CAnimEvent *self, CAnimEvent **list) {
	CHECK(list);
	CHECK(!(self->flags & ANIMEVENT_INSERTED));
	self->flags |= ANIMEVENT_INSERTED;
	CAnimEvent *cur;
	while ((cur = *list) != NULL) {
		if (cur->t > self->t || (cur->t == self->t && (uint32_t)self->id < (uint32_t)cur->id)) {
			break;
		}
		list = &cur->next;
	}
	self->next = cur;
	*list = self;
}

CAnimEvent *AnimEvents_Add(CAnimEvent **list, CAnimEvent *ev) {
	CAnimEvent_Insert(ev, list);
	return ev;
}
