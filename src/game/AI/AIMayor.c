/*
 * AI/AIMayor.cpp of the game: the AI of the mayor (cAIChar with the move control of cAIChar): the action button at a
 * target it sees directly within the distances of the action. See include/scrap/ai.h.
 */
#include "scrap/ai.h"
#include "scrap/aichar.h"
#include "scrap/walkcharentity.h"

#define CAIMAYOR_SIZE sizeof(cAIMayor)

void *cAIMayor_ctor(void *mem, cEntity *e, int *arg) {
	cAIMayor *self = mem;

	cAIChar_ctor(mem, e, arg);
	self->vftable = CAIMAYOR_VFTABLE;
	return self;
}

cAIMayor *cAIMayor_Delete(cAIMayor *self, byte flags) {
	self->vftable = CAIMAYOR_VFTABLE;
	cAIChar_dtor(self);
	if (flags & 1) {
		operator_delete_sized(self, CAIMAYOR_SIZE);
	}
	return self;
}

void cAIMayor_CheckActDist(cAIMayor *self) {
	float dist = AIChar_TargetDistFlat(self);

	self->ctrl->act = self->vftable->GetDirect2(self) && AIChar_Action0(self->ent)->priority <= 0x65 &&
		dist > self->actMin && self->actMax > dist;
}

void cAIMayor_SetDists(cAIMayor *self) {
	if (self->state == 2) {
		self->actMin = 0.0f;
		self->actMax = 1000.0f;
	} else if (self->state != 3) {
		cAIChar_SetDists(self);
		return;
	}
	self->chaseFar = 600.0f;
	self->chaseNear = 400.0f;
}
