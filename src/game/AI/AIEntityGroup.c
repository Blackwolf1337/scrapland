/*
 * AI/AIEntityGroup.cpp of the game: cAIEntityGroup (include/scrap/ai.h), the base of the vehicle group and the
 * character group. It holds up to 20 entities and the time since one of its AIs took a group state; the next AI may
 * take one once the time reached TimeBetwChangeStateAI.
 */
#include "scrap/ai.h"

void *__fastcall cAIEntityGroup_ctor(void *self) {
	cAIEntityGroup *g = self;

	g->vftable = CAIENTITYGROUP_VFTABLE;
	g->num = 0;
	g->b58 = 0;
	g->changeTime = g_TimeBetwChangeStateAI;
	return g;
}

void __fastcall cAIEntityGroup_dtor(void *self) {
	((cAIEntityGroup *)self)->vftable = CAIENTITYGROUP_VFTABLE;
}

cAIEntityGroup *__thiscall cAIEntityGroup_Delete(cAIEntityGroup *self, byte flags) {
	self->vftable = CAIENTITYGROUP_VFTABLE;
	if (flags & 1) {
		operator_delete_sized(self, CAIENTITYGROUP_SIZE);
	}
	return self;
}

void __thiscall cAIEntityGroup_Clear(cAIEntityGroup *self) {
	self->num = 0;
}

byte __thiscall cAIEntityGroup_Add(cAIEntityGroup *self, cEntity *enemy) {
	CHECK(enemy != NULL);
	if (self->num == CAIENTITYGROUP_MAX) {
		return 0;
	}
	self->ents[self->num] = enemy;
	self->num++;
	return 1;
}

cEntity *__thiscall cAIEntityGroup_Get(cAIEntityGroup *self, int numEnt) {
	CHECK(numEnt < self->num);
	return self->ents[numEnt];
}

int __thiscall cAIEntityGroup_Num(cAIEntityGroup *self) {
	return self->num;
}

byte __thiscall cAIEntityGroup_CanChange(cAIEntityGroup *self) {
	return !(g_TimeBetwChangeStateAI > self->changeTime);
}

void __thiscall cAIEntityGroup_ResetChange(cAIEntityGroup *self) {
	self->changeTime = 0.0f;
}

void __thiscall cAIEntityGroup_AllowChange(cAIEntityGroup *self) {
	self->changeTime = g_TimeBetwChangeStateAI;
}

void __thiscall cAIEntityGroup_Update(cAIEntityGroup *self, float dt) {
	self->changeTime = dt + self->changeTime;
}
