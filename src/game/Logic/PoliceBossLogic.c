/*
 * Logic/PoliceBossLogic.cpp of the game: the police boss, a cGenCharLocalLogic (include/scrap/logic.h) with its
 * own walks and a special action instead of an attack target: every PoliceBossMinSpecialActionTime /
 * PoliceBossMaxSpecialActionTime it tries it while a character of the second GenChar list is near and the boss can
 * be seen.
 */
#include "scrap/chartype.h"
#include "scrap/logic.h"
#include "scrap/scrapmodel.h"
#include "scrap/weapons.h"

#define CPOLICEBOSSLOCALLOGIC_VFTABLE GAME_VFTABLE(cPoliceBossLocalLogic_vftable)
#define g_PoliceBossMinSpecialActionTime GAME_GLOBAL_IN(float, GVar_PoliceBossMinSpecialActionTime, 16)
#define g_PoliceBossMaxSpecialActionTime GAME_GLOBAL_IN(float, GVar_PoliceBossMaxSpecialActionTime, 16)

cPoliceBossLocalLogic *__thiscall cPoliceBossLocalLogic_ctor(cPoliceBossLocalLogic *self, cEntity *parent) {
	cGenCharLocalLogic_ctor(self, parent, 0);
	self->base.vftable = CPOLICEBOSSLOCALLOGIC_VFTABLE;
	return self;
}

cPoliceBossLocalLogic *__thiscall cPoliceBossLocalLogic_Delete(cPoliceBossLocalLogic *self, byte flags) {
	self->base.vftable = CPOLICEBOSSLOCALLOGIC_VFTABLE;
	cGenCharLocalLogic_dtor(self);
	if (flags & 1) {
		operator_delete_sized(self, sizeof(*self));
	}
	return self;
}

/* vf30: no target; when the search time is over the special action of the boss, near a character it can see */
cEntity *__thiscall cPoliceBossLocalLogic_FindTarget(cPoliceBossLocalLogic *self) {
	vec3_t tmp;

	if (!(0.0f > self->searchTime)) {
		return NULL;
	}
	cEntity *p = self->base.parent;
	self->searchTime = g_PoliceBossMinSpecialActionTime * 0.5f;
	if (!CGameModel_IsVisible(Obj_Vtbl(p)->GetModel(p))) {
		return NULL;
	}
	cEntity *e = cEntityList_Nearest(g_pWorld->logic.genChars.police, p->vftable->GetPos(p, &tmp), p, NULL);
	if (!e || !e->vftable->IsType(e, TYEN_WALKCHAR)) {
		return NULL;
	}
	float lo = g_PoliceBossMinSpecialActionTime;
	float hi = g_PoliceBossMaxSpecialActionTime;
	self->searchTime = (float)(Weap_Rand01() * (double)(hi - lo) + (double)lo);
	ChType_PoliceBossTryAction(p);
	return NULL;
}

/* vf28 and vf27: the AI walks in alert or relaxed */
void __thiscall cPoliceBossLocalLogic_ToAlert(cPoliceBossLocalLogic *self, cAIChar *ai) {
	(void)self;
	ai->vftable->SetState(ai, 1, (double)g_GenCharAlertSpeed, 0);
}

void __thiscall cPoliceBossLocalLogic_ToRelax(cPoliceBossLocalLogic *self, cAIChar *ai) {
	(void)self;
	ai->vftable->SetState(ai, 1, (double)g_GenCharRelaxSpeed, 0);
}

/* vf17 */
void __thiscall cPoliceBossLocalLogic_Update(cPoliceBossLocalLogic *self, float dt) {
	cGenCharLocalLogic_Update(self, dt);
}
