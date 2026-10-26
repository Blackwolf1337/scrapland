/*
 * Engine/EventoAnim.cpp of the engine3d: the events of the animations (CAnimEvent, include/scrap/controlanim.h), a
 * list sorted by the time.
 */
#include "scrap/controlanim.h"

#define ANIMEVENT_INSERTADO 1		/* flags: in a list */

CAnimEvent *__thiscall CAnimEvent_ctor2(CAnimEvent *self, float t, int id) {
	self->id = id;
	self->t = t;
	self->arg = 0;
	self->sig = NULL;
	self->flags = 0;
	return self;
}

CAnimEvent *__thiscall CAnimEvent_ctor(CAnimEvent *self, float t, int id, int arg) {
	self->id = id;
	self->arg = arg;
	self->t = t;
	self->sig = NULL;
	self->flags = 0;
	return self;
}

CAnimEvent *__thiscall CAnimEvent_ctorAdd(CAnimEvent *self, float t, int id, int arg, CAnimEvent **list) {
	self->id = id;
	self->t = t;
	self->arg = arg;
	self->sig = NULL;
	self->flags = 0;
	CAnimEvent_Insert(self, list);
	return self;
}

void __fastcall CAnimEvent_dtor(CAnimEvent *self) {
	CAnimEvent *p = self->sig;
	if (p) {
		CAnimEvent_dtor(p);
		operator_delete_sized(p, sizeof(CAnimEvent));
		self->sig = NULL;
	}
}

CAnimEvent *__thiscall CAnimEvent_Delete(CAnimEvent *self, byte flags) {
	if (self->sig) {
		CAnimEvent_Delete(self->sig, 1);
		self->sig = NULL;
	}
	if (flags & 1) {
		operator_delete_sized(self, sizeof(CAnimEvent));
	}
	return self;
}

void __thiscall CAnimEvent_Insert(CAnimEvent *self, CAnimEvent **list) {
	CHECK(list);
	CHECK(!(self->flags & ANIMEVENT_INSERTADO));
	self->flags |= ANIMEVENT_INSERTADO;
	CAnimEvent *cur;
	while ((cur = *list) != NULL) {
		if (cur->t > self->t || (cur->t == self->t && (uint32_t)self->id < (uint32_t)cur->id)) {
			break;
		}
		list = &cur->sig;
	}
	self->sig = cur;
	*list = self;
}

CAnimEvent *__thiscall AnimEventos_Add(CAnimEvent **list, CAnimEvent *ev) {
	CAnimEvent_Insert(ev, list);
	return ev;
}
