/*
 * AI/AIBankMaster.cpp of the game: the AI of the bank master (cAIBankMaster, a cAIChar). The attack check and the
 * state changes are the ones of the director (AIBankDirector.c). See include/scrap/ai.h.
 */
#include "scrap/ai.h"
#include "scrap/aichar.h"
#include "scrap/walkcharentity.h"

#define CAIBANK_SIZE 0x384

void *__thiscall cAIBankMaster_ctor(void *mem, cEntity *e, int *arg) {
	cAIChar *self = mem;

	cAIChar_ctor(mem, e, arg);
	self->vftable = Game_Ptr(CAIBANKMASTER_VFTABLE);
	cAIChar_NewControl(self, CAICHARCONTROL_SIZE, cAIControlBankMaster_ctor, e, arg);
	return self;
}

void __fastcall cAIBankMaster_dtor(cAIChar *self) {
	self->vftable = Game_Ptr(CAIBANKMASTER_VFTABLE);
	cAIChar_dtor(self);
}

cAIChar *__thiscall cAIBankMaster_Delete(cAIChar *self, byte flags) {
	self->vftable = Game_Ptr(CAIBANKMASTER_VFTABLE);
	cAIChar_dtor(self);
	if (flags & 1) {
		operator_delete_sized(self, CAIBANK_SIZE);
	}
	return self;
}

void __thiscall cAIBankMaster_SetRanges(cAIBank *self) {
	cAIChar *c = &self->base;

	if (c->state == 3) {
		c->chaseNear = 300.0f;
		c->chaseFar = 550.0f;
		return;
	}
	if (c->state != 7) {
		cAIChar_SetDists(c);
		return;
	}
	c->chaseNear = 0.0f;
	c->chaseFar = 550.0f;
	c->actMin = 0.0f;
	c->actMax = g_BankMasterVacumDist;
}
