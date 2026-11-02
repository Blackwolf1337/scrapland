/*
 * AI/AIBankMaster.cpp of the game: the AI of the bank master (cAIBankMaster, a cAIChar). The attack check and the
 * state changes are the ones of the director (AIBankDirector.c). See include/scrap/ai.h.
 */
#include "scrap/ai.h"
#include "scrap/aichar.h"
#include "scrap/walkcharentity.h"

void *cAIBankMaster_ctor(void *mem, cEntity *e, int *arg) {
	cAIBankMaster *self = mem;

	cAIChar_ctor(mem, e, arg);
	self->base.vftable = CAIBANKMASTER_VFTABLE;
	cAIChar_NewControl(&self->base, CAICHARCONTROL_SIZE, cAIControlBankMaster_ctor, e, arg);
	return self;
}

cAIBankMaster *cAIBankMaster_Delete(cAIBankMaster *self, byte flags) {
	self->base.vftable = CAIBANKMASTER_VFTABLE;
	cAIChar_dtor(&self->base);
	if (flags & 1) {
		operator_delete_sized(self, sizeof(cAIBankMaster));
	}
	return self;
}

void cAIBankMaster_SetRanges(cAIBankMaster *self) {
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
