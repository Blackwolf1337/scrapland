/*
 * AI/AIBankDirector.cpp of the game: the AI of the bank director (cAIBankDirector, a cAIChar). Its attack check and
 * its state changes are the same code as the ones of the bank master (AIBankMaster.c). See include/scrap/ai.h.
 */
#include "scrap/actions.h"
#include "scrap/ai.h"
#include "scrap/aichar.h"
#include "scrap/walkcharentity.h"

#define CAIBANK_SIZE 0x384

void *__thiscall cAIBankDirector_ctor(void *mem, cEntity *e, int *arg) {
	cAIBankDirector *self = mem;

	cAIChar_ctor(mem, e, arg);
	self->base.vftable = CAIBANKDIRECTOR_VFTABLE;
	cAIChar_NewControl(&self->base, CAICHARCONTROL_SIZE, cAIControlBankDirector_ctor, e, arg);
	return self;
}

void __fastcall cAIBankDirector_dtor(cAIBankDirector *self) {
	self->base.vftable = CAIBANKDIRECTOR_VFTABLE;
	cAIChar_dtor(&self->base);
}

cAIBankDirector *__thiscall cAIBankDirector_Delete(cAIBankDirector *self, byte flags) {
	self->base.vftable = CAIBANKDIRECTOR_VFTABLE;
	cAIChar_dtor(&self->base);
	if (flags & 1) {
		operator_delete_sized(self, CAIBANK_SIZE);
	}
	return self;
}

void __thiscall cAIBank_CheckAttack(cAIBank *self) {
	cAIChar *c = &self->base;
	cAICharControl *ctl = c->control;
	byte go;
	vec3_t a, b;

	if (AICC_Vtbl(ctl)->GetWalkDir(ctl) == 0 && AICC_Vtbl(ctl)->GetOrbitSide(ctl) == 0 && !c->ctrl->act) {
		go = 1;
	} else if (c->ctrl->act) {
		go = 1;
	} else {
		go = self->money > WChar(c->ent)->money;
	}
	cEntity *tgt = c->base.target.target;
	const vec3_t *op = c->ent->vftable->GetPos(c->ent, &a);
	const vec3_t *tp = tgt->vftable->GetPos(tgt, &b);
	float dx = tp->x - op->x;
	float dz = tp->z - op->z;
	float dist = (float)__builtin_sqrt((double)(dx * dx + 0.0f + dz * dz));
	if (go && AICC_Vtbl(c->control)->IsDirect2(c->control)) {
		tgt = c->base.target.target;
		tp = tgt->vftable->GetPos(tgt, &b);
		op = c->ent->vftable->GetPos(c->ent, &a);
		if ((double)g_MaxHeightChaseCharAI > __builtin_fabs((double)(tp->y - op->y)) &&
			AIChar_Action0(c->ent)->priority <= 0x65 && dist > c->actMin && c->actMax > dist) {
			c->ctrl->act = 1;
			return;
		}
	}
	c->ctrl->act = 0;
}

void __thiscall cAIBankDirector_SetRanges(cAIBankDirector *self) {
	cAIChar *c = &self->base;

	if (c->state != 3) {
		if (c->state != 7) {
			cAIChar_SetDists(c);
			return;
		}
		c->actMin = 0.0f;
		c->actMax = 1000.0f;
	}
	c->chaseFar = 550.0f;
	c->chaseNear = 300.0f;
}

byte cAIBank_SetState(cAIBank *self, int state, ...) {
	cAIChar *c = &self->base;
	va_list ap;

	AIChar_DebugState(c, "Demanda externa de CAMBIO de ESTADO -> %s", state);
	if (state == 2) {
		self->money = WChar(c->ent)->money;
		state = 7;
	}
	if (!c->vftable->IsGroupState(c, state) || g_CharGroup->vftable->CanChange(g_CharGroup)) {
		c->vftable->BeginChange(c, state);
	}
	va_start(ap, state);
	byte r = cAIChar_ChangeState(c, state, ap);
	va_end(ap);
	return r;
}

byte cAIBank_SetStateIntern(cAIBank *self, int state, ...) {
	cAIChar *c = &self->base;
	va_list ap;

	AIChar_DebugState(c, "Demanda interna de CAMBIO de ESTADO -> %s", state);
	if (c->vftable->IsGroupState(c, state)) {
		g_CharGroup->vftable->AllowChange(g_CharGroup);
	}
	if (state == 2) {
		self->money = WChar(c->ent)->money;
		state = 7;
	}
	c->vftable->BeginChange(c, state);
	va_start(ap, state);
	byte r = cAIChar_ChangeState(c, state, ap);
	va_end(ap);
	return r;
}
