/*
 * AI/AISentinel.cpp of the game: the AI of the sentinel (a cAIChar with cAIControlSentinel): the action button within
 * the distances of the action while its action lets it walk. See include/scrap/ai.h.
 */
#include "scrap/ai.h"
#include "scrap/aichar.h"
#include "scrap/walkcharentity.h"

#define CAISENTINEL_SIZE 0x380

void *cAISentinel_ctor(void *mem, cEntity *e, int *arg) {
	cAISentinel *self = mem;

	cAIChar_ctor(mem, e, arg);
	self->vftable = CAISENTINEL_VFTABLE;
	cAIChar_NewControl(self, CAICONTROLSENTINEL_SIZE, cAIControlSentinel_ctor, e, arg);
	self->canTalk = 0;
	return self;
}

cAISentinel *cAISentinel_Delete(cAISentinel *self, byte flags) {
	self->vftable = CAISENTINEL_VFTABLE;
	cAIChar_dtor(self);
	if (flags & 1) {
		operator_delete_sized(self, CAISENTINEL_SIZE);
	}
	return self;
}

void cAISentinel_CheckActDist(cAISentinel *self) {
	float dist = AIChar_TargetDist(self);

	self->ctrl->act = AIChar_Action0(self->ent)->useFlyDisp && AIChar_Action0(self->ent)->priority <= 0x65 &&
		dist > self->actMin && self->actMax > dist;
}

void cAISentinel_SetDists(cAISentinel *self) {
	float d;

	if (self->state == 2) {
		d = WChar(self->ent)->wl.prim.radius * 1.25f + WChar(self->base.target.target)->wl.prim.radius;
		self->actMin = 0.0f;
		self->actMax = 2500.0f;
		self->chaseFar = 750.0f;
		self->chaseNear = d;
		return;
	}
	if (self->state != 3) {
		cAIChar_SetDists(self);
		return;
	}
	d = WChar(self->ent)->wl.prim.radius * 1.25f + WChar(self->base.target.target)->wl.prim.radius;
	self->chaseFar = 750.0f;
	self->chaseNear = d;
}
