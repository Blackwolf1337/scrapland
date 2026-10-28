/*
 * AI/AIPolice.cpp of the game: the AI of the police (a cAIChar with the move control of cAIChar): the action button
 * within the distances of the action, which follow the radii of the two characters. See include/scrap/ai.h.
 */
#include "scrap/ai.h"
#include "scrap/aichar.h"
#include "scrap/walkcharentity.h"

#define CAIPOLICE_SIZE 0x380

void *__thiscall cAIPolice_ctor(void *mem, cEntity *e, int *arg) {
	cAIChar *self = mem;

	cAIChar_ctor(mem, e, arg);
	self->vftable = CAIPOLICE_VFTABLE;
	return self;
}

void __fastcall cAIPolice_dtor(cAIChar *self) {
	self->vftable = CAIPOLICE_VFTABLE;
	cAIChar_dtor(self);
}

cAIChar *__thiscall cAIPolice_Delete(cAIChar *self, byte flags) {
	self->vftable = CAIPOLICE_VFTABLE;
	cAIChar_dtor(self);
	if (flags & 1) {
		operator_delete_sized(self, CAIPOLICE_SIZE);
	}
	return self;
}

void __thiscall cAIPolice_CheckActDist(cAIChar *self) {
	float dist = AIChar_TargetDistFlat(self);

	self->ctrl->act = AIChar_Action0(self->ent)->priority <= 0x65 && self->actMax > dist && dist > self->actMin;
}

void __thiscall cAIPolice_SetDists(cAIChar *self) {
	float d;

	if (self->state == 2) {
		d = WChar(self->target.target)->prim.radius * 1.5f + WChar(self->ent)->prim.radius;
		self->chaseNear = d;
		d = d + 100.0f;
		self->actMin = self->chaseNear;
		self->chaseFar = d;
		self->actMax = self->chaseFar;
		return;
	}
	if (self->state != 3) {
		cAIChar_SetDists(self);
		return;
	}
	d = WChar(self->ent)->prim.radius * 3.5f + WChar(self->target.target)->prim.radius;
	self->chaseNear = d;
	self->chaseFar = d + 100.0f;
}
