/*
 * AI/AIEntityGroup.cpp of the game: cAIEntityGroup (include/scrap/ai.h), the base of the vehicle group and the
 * character group. It holds up to 20 entities and the time since one of its AIs took a group state; the next AI may
 * take one once the time reached TimeBetwChangeStateAI.
 */
#include "scrap/ai.h"
#include "scrap/staticinit.h"

void *cAIEntityGroup_ctor(void *self) {
	cAIEntityGroup *g = self;

	g->vftable = CAIENTITYGROUP_VFTABLE;
	g->num = 0;
	g->unused_58 = 0;
	g->changeTime = g_TimeBetwChangeStateAI;
	return g;
}

void cAIEntityGroup_dtor(void *self) {
	((cAIEntityGroup *)self)->vftable = CAIENTITYGROUP_VFTABLE;
}

cAIEntityGroup *cAIEntityGroup_Delete(cAIEntityGroup *self, byte flags) {
	self->vftable = CAIENTITYGROUP_VFTABLE;
	if (flags & 1) {
		operator_delete_sized(self, CAIENTITYGROUP_SIZE);
	}
	return self;
}

void cAIEntityGroup_Clear(cAIEntityGroup *self) {
	self->num = 0;
}

byte cAIEntityGroup_Add(cAIEntityGroup *self, cEntity *enemy) {
	CHECK(enemy != NULL);
	if (self->num == CAIENTITYGROUP_MAX) {
		return 0;
	}
	self->ents[self->num] = enemy;
	self->num++;
	return 1;
}

cEntity *cAIEntityGroup_Get(cAIEntityGroup *self, int numEnt) {
	CHECK(numEnt < self->num);
	return self->ents[numEnt];
}

int cAIEntityGroup_Num(cAIEntityGroup *self) {
	return self->num;
}

byte cAIEntityGroup_CanChange(cAIEntityGroup *self) {
	return !(g_TimeBetwChangeStateAI > self->changeTime);
}

void cAIEntityGroup_ResetChange(cAIEntityGroup *self) {
	self->changeTime = 0.0f;
}

void cAIEntityGroup_AllowChange(cAIEntityGroup *self) {
	self->changeTime = g_TimeBetwChangeStateAI;
}

void cAIEntityGroup_Update(cAIEntityGroup *self, float dt) {
	self->changeTime = dt + self->changeTime;
}

/* the vtable of cAIEntityGroup */
const cAIEntityGroupVtbl cAIEntityGroup_vftable = {
	.Delete = cAIEntityGroup_Delete,
	.Clear = cAIEntityGroup_Clear,
	.Add = cAIEntityGroup_Add,
	.Get = cAIEntityGroup_Get,
	.Num = cAIEntityGroup_Num,
	.CanChange = cAIEntityGroup_CanChange,
	.ResetChange = cAIEntityGroup_ResetChange,
	.AllowChange = cAIEntityGroup_AllowChange,
	.Update = cAIEntityGroup_Update,
};

cGVar GVar_TimeBetwChangeStateAI;

/* the dynamic initializers of the globals here, run in the order of src/standalone/crtxc.inc */
INIT_GVAR_FLOAT(Init_GVar_TimeBetwChangeStateAI, &GVar_TimeBetwChangeStateAI, "TimeBetwChangeStateAI", 4, "Time between change state", 0.05f)
