/*
 * AI/AIBankMaster.cpp of the game: the AI of the bank master (cAIBankMaster, a cAIChar). The attack check and the
 * state changes are the ones of the director (AIBankDirector.c). See include/scrap/ai.h.
 */
#include "scrap/ai.h"
#include "scrap/aichar.h"
#include "scrap/walkcharentity.h"

#define CAIBANK_SIZE 0x384
#define CAICONTROL_SIZE 0x360

void *__thiscall cAIBankMaster_ctor(void *mem, cEntity *e, int *arg) {
	cAICharView *self = mem;

	cAIChar_ctor(mem, e, arg);
	*(const void **)self = Game_Ptr(CAIBANKMASTER_VFTABLE);
	if (self->control) {
		((void *(__thiscall *)(void *, byte))AI_VSlot(self->control, 0))(self->control, 1);
	}
	void *ctl = operator_new(CAICONTROL_SIZE);
	self->control = ctl ? cAIControlBankMaster_ctor(ctl, e, self, arg) : NULL;
	CHECK(self->control);
	return self;
}

void __fastcall cAIBankMaster_dtor(void *self) {
	*(const void **)self = Game_Ptr(CAIBANKMASTER_VFTABLE);
	cAIChar_dtor((cAIChar *)self);
}

void *__thiscall cAIBankMaster_Delete(void *self, byte flags) {
	*(const void **)self = Game_Ptr(CAIBANKMASTER_VFTABLE);
	cAIChar_dtor((cAIChar *)self);
	if (flags & 1) {
		operator_delete_sized(self, CAIBANK_SIZE);
	}
	return self;
}

void __thiscall cAIBankMaster_SetRanges(cAIBank *self) {
	cAICharView *c = &self->base;

	if (c->state == 3) {
		c->rangeMin = 300.0f;
		c->rangeMax = 550.0f;
		return;
	}
	if (c->state != 7) {
		cAIChar_SetDists((cAIChar *)self);
		return;
	}
	c->rangeMin = 0.0f;
	c->rangeMax = 550.0f;
	c->attackMin = 0.0f;
	c->attackMax = g_BankMasterVacumDist;
}
