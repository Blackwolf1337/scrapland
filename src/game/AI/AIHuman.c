/*
 * AI/AIHuman.cpp of the game: the AI of the humans (cAIHuman, a cAIChar with the move control of cAIChar) that do
 * not talk and chase nearer. See include/scrap/ai.h.
 */
#include "scrap/ai.h"
#include "scrap/aichar.h"
#include "scrap/walkcharentity.h"

#define CAIHUMAN_SIZE 0x380

void *__thiscall cAIHuman_ctor(void *mem, cEntity *e, int *arg) {
	cAIChar *self = mem;

	cAIChar_ctor(mem, e, arg);
	self->vftable = CAIHUMAN_VFTABLE;
	self->canTalk = 0;
	return self;
}

void __fastcall cAIHuman_dtor(cAIChar *self) {
	self->vftable = CAIHUMAN_VFTABLE;
	cAIChar_dtor(self);
}

cAIChar *__thiscall cAIHuman_Delete(cAIChar *self, byte flags) {
	self->vftable = CAIHUMAN_VFTABLE;
	cAIChar_dtor(self);
	if (flags & 1) {
		operator_delete_sized(self, CAIHUMAN_SIZE);
	}
	return self;
}

void __thiscall cAIHuman_SetDists(cAIChar *self) {
	if (self->state != 3) {
		cAIChar_SetDists(self);
		return;
	}
	self->chaseNear = 200.0f;
	self->chaseFar = 300.0f;
}
