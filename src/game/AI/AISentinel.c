/*
 * AI/AISentinel.cpp of the game: the AI of the sentinel (a cAIChar with cAIControlSentinel): the action button within
 * the distances of the action while its action lets it walk. See include/scrap/ai.h.
 */
#include "scrap/ai.h"
#include "scrap/aichar.h"
#include "scrap/walkcharentity.h"

#define CAISENTINEL_SIZE 0x380

void *__thiscall cAISentinel_ctor(void *mem, cEntity *e, int *arg) {
	cAIChar *self = mem;

	cAIChar_ctor(mem, e, arg);
	self->vftable = Game_Ptr(CAISENTINEL_VFTABLE);
	cAIChar_NewControl(self, CAICONTROLSENTINEL_SIZE, cAIControlSentinel_ctor, e, arg);
	self->canTalk = 0;
	return self;
}

void __fastcall cAISentinel_dtor(cAIChar *self) {
	self->vftable = Game_Ptr(CAISENTINEL_VFTABLE);
	cAIChar_dtor(self);
}

cAIChar *__thiscall cAISentinel_Delete(cAIChar *self, byte flags) {
	self->vftable = Game_Ptr(CAISENTINEL_VFTABLE);
	cAIChar_dtor(self);
	if (flags & 1) {
		operator_delete_sized(self, CAISENTINEL_SIZE);
	}
	return self;
}

void __thiscall cAISentinel_CheckActDist(cAIChar *self) {
	float dist = AIChar_TargetDist(self);

	self->ctrl->act = AIChar_Action0(self->ent)->walk && AIChar_Action0(self->ent)->priority <= 0x65 &&
		dist > self->actMin && self->actMax > dist;
}

void __thiscall cAISentinel_SetDists(cAIChar *self) {
	float d;

	if (self->state == 2) {
		d = WChar(self->ent)->prim.radius * 1.25f + WChar(self->target.target)->prim.radius;
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
	d = WChar(self->ent)->prim.radius * 1.25f + WChar(self->target.target)->prim.radius;
	self->chaseFar = 750.0f;
	self->chaseNear = d;
}
