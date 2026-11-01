/*
 * AI/AINurse.cpp of the game: the AI of the nurse (cAINurse, a cAIChar with cAIControlNurse). It uses the action
 * button on a dazed target: at once when the attack (state 2) just began and the target is within actMax, else from
 * beyond actMax up to DistActionNurseAI. See include/scrap/ai.h.
 */
#include "scrap/ai.h"
#include "scrap/aichar.h"
#include "scrap/walkcharentity.h"

#define CAINURSE_SIZE 0x384

void *__thiscall cAINurse_ctor(void *mem, cEntity *e, int *arg) {
	cAINurse *self = mem;
	cAIChar *c = &self->base;

	cAIChar_ctor(mem, e, arg);
	c->vftable = CAINURSE_VFTABLE;
	cAIChar_NewControl(c, CAICHARCONTROL_SIZE, cAIControlNurse_ctor, e, arg);
	self->fresh = 0;
	c->canTalk = 0;
	return self;
}

void __fastcall cAINurse_dtor(cAIChar *self) {
	self->vftable = CAINURSE_VFTABLE;
	cAIChar_dtor(self);
}

cAIChar *__thiscall cAINurse_Delete(cAIChar *self, byte flags) {
	self->vftable = CAINURSE_VFTABLE;
	cAIChar_dtor(self);
	if (flags & 1) {
		operator_delete_sized(self, CAINURSE_SIZE);
	}
	return self;
}

void __thiscall cAINurse_CheckActDist(cAINurse *self) {
	cAIChar *c = &self->base;
	float dist = AIChar_TargetDist(c);

	if (AIChar_Action0(c->ent)->useFlyDisp && AIChar_ActionIs(c->base.target.target, "Dazed") &&
		((g_DistActionNurseAI > dist && dist > c->actMax) || (c->actMax > dist && self->fresh))) {
		self->fresh = 0;
		c->ctrl->act = 1;
		return;
	}
	self->fresh = 1;
	c->ctrl->act = 0;
}

void __thiscall cAINurse_SetDists(cAINurse *self) {
	cAIChar *c = &self->base;

	if (c->state != 2) {
		cAIChar_SetDists(c);
		return;
	}
	float nearD = WChar(c->ent)->wl.prim.radius * 1.1f + WChar(c->base.target.target)->wl.prim.radius;
	c->chaseNear = nearD;
	float farD = nearD + g_OffsetMoveNurseAI;
	c->actMin = c->chaseNear;
	c->chaseFar = farD;
	c->actMax = c->chaseFar;
}

byte cAINurse_SetState(cAINurse *self, int state, ...) {
	cAIChar *c = &self->base;
	va_list ap;

	AIChar_DebugState(c, "Demanda externa de CAMBIO de ESTADO -> %s", state);
	if (!c->vftable->IsGroupState(c, state) || g_CharGroup->vftable->CanChange(g_CharGroup)) {
		c->vftable->BeginChange(c, state);
		if (c->state == 2) {
			self->fresh = 1;
		}
	}
	va_start(ap, state);
	byte r = cAIChar_ChangeState(c, state, ap);
	va_end(ap);
	return r;
}

byte cAINurse_SetStateIntern(cAINurse *self, int state, ...) {
	cAIChar *c = &self->base;
	va_list ap;

	AIChar_DebugState(c, "Demanda interna de CAMBIO de ESTADO -> %s", state);
	if (c->vftable->IsGroupState(c, state)) {
		g_CharGroup->vftable->AllowChange(g_CharGroup);
	}
	c->vftable->BeginChange(c, state);
	if (c->state == 2) {
		self->fresh = 1;
	}
	va_start(ap, state);
	byte r = cAIChar_ChangeState(c, state, ap);
	va_end(ap);
	return r;
}
