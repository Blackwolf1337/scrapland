/*
 * Logic/PoliceBossLogic.cpp of the game: the police boss, a cGenCharLocalLogic (include/scrap/logic.h) with its
 * own walks and a special action instead of an attack target: every PoliceBossMinSpecialActionTime /
 * PoliceBossMaxSpecialActionTime it tries it while a character of the second GenChar list is near and the boss can
 * be seen.
 */
#include "scrap/gvar.h"
#include "scrap/chartype.h"
#include "scrap/logic.h"
#include "scrap/scrapmodel.h"
#include "scrap/weapons.h"

extern const cGenCharLocalLogicVtbl cPoliceBossLocalLogic_vftable;
#define CPOLICEBOSSLOCALLOGIC_VFTABLE (&cPoliceBossLocalLogic_vftable.base)
extern cGVar GVar_PoliceBossMinSpecialActionTime;
#define g_PoliceBossMinSpecialActionTime (GVar_PoliceBossMinSpecialActionTime.value.f)
extern cGVar GVar_PoliceBossMaxSpecialActionTime;
#define g_PoliceBossMaxSpecialActionTime (GVar_PoliceBossMaxSpecialActionTime.value.f)

cPoliceBossLocalLogic *cPoliceBossLocalLogic_ctor(cPoliceBossLocalLogic *self, cEntity *parent) {
	cGenCharLocalLogic_ctor(self, parent, 0);
	self->base.vftable = CPOLICEBOSSLOCALLOGIC_VFTABLE;
	return self;
}

cLocalLogic *cPoliceBossLocalLogic_Delete(cLocalLogic *elem, byte flags) {
	cPoliceBossLocalLogic *self = (cPoliceBossLocalLogic *)elem;
	self->base.vftable = CPOLICEBOSSLOCALLOGIC_VFTABLE;
	cGenCharLocalLogic_dtor(self);
	if (flags & 1) {
		operator_delete_sized(self, sizeof(*self));
	}
	return elem;
}

/* vf30: no target; when the search time is over the special action of the boss, near a character it can see */
cEntity *cPoliceBossLocalLogic_FindTarget(cLocalLogic *elem) {
	cPoliceBossLocalLogic *self = (cPoliceBossLocalLogic *)elem;
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
void cPoliceBossLocalLogic_ToAlert(cLocalLogic *base, cAIChar *ai) {
	cPoliceBossLocalLogic *self = (cPoliceBossLocalLogic *)base;
	(void)self;
	ai->vftable->SetState(ai, 1, (double)g_GenCharAlertSpeed, 0);
}

void cPoliceBossLocalLogic_ToRelax(cLocalLogic *base, cAIChar *ai) {
	cPoliceBossLocalLogic *self = (cPoliceBossLocalLogic *)base;
	(void)self;
	ai->vftable->SetState(ai, 1, (double)g_GenCharRelaxSpeed, 0);
}

/* vf17 */
void cPoliceBossLocalLogic_Update(cLocalLogic *base, float dt) {
	cPoliceBossLocalLogic *self = (cPoliceBossLocalLogic *)base;
	cGenCharLocalLogic_Update(&self->base.logic, dt);
}

/* the vtable of cPoliceBossLocalLogic */
const cGenCharLocalLogicVtbl cPoliceBossLocalLogic_vftable = {
	.base.base.Delete = cPoliceBossLocalLogic_Delete,
	.base.base.GetName = cBaseLocalLogic_GetName,
	.base.base.Vf02 = cLocalLogic_Vf02True,
	.base.base.VarI = cGenCharLocalLogic_VarI,
	.base.base.VarF = cBaseLocalLogic_VarF,
	.base.base.VarS = cGenCharLocalLogic_VarS,
	.base.base.VarV = cBaseLocalLogic_VarV,
	.base.UpdateBusy = cBaseLocalLogic_UpdateBusy,
	.base.Refresh = cBaseLocalLogic_Refresh,
	.base.Listens = cLocalLogic_ListensTrue,
	.base.OnHit = cGenCharLocalLogic_OnHit,
	.base.OnSeen = cGenCharLocalLogic_OnSeen,
	.base.OnAttack = cGenCharLocalLogic_OnRobbery,
	.base.OnShot = cBaseLocalLogic_OnShot,
	.base.OnHammer = cLocalLogic_OnHammerNop,
	.base.GivesMoney = cGenCharLocalLogic_GivesMoney,
	.base.OnAlarm = cGenCharLocalLogic_OnAlarm,
	.base.Update = cPoliceBossLocalLogic_Update,
	.base.SetOriginFrom = cBaseLocalLogic_SetOriginFrom,
	.base.SetState = cGenCharLocalLogic_SetState,
	.base.SetIdle = cBaseLocalLogic_SetIdle,
	.base.SetActionRadius = cBaseLocalLogic_SetActionRadius,
	.base.SetOrigin = cBaseLocalLogic_SetOrigin,
	.base.SetOriginAng = cBaseLocalLogic_SetOriginAng,
	.base.RestartState = cGenCharLocalLogic_RestartState,
	.CanAttack = cGenCharLocalLogic_CanAttack,
	.UpdateView = cGenCharLocalLogic_UpdateView,
	.ToRelax = cPoliceBossLocalLogic_ToRelax,
	.ToAlert = cPoliceBossLocalLogic_ToAlert,
	.Accuse = Logic_SetState2,
	.FindTarget = cPoliceBossLocalLogic_FindTarget,
	.TargetDone = cLocalLogic_TargetDoneTrue,
	.Work = cLocalLogic_WorkNop,
	.Vf33 = cLocalLogic_Vf33Nop,
	.GoTo = cGenCharLocalLogic_GoTo,
	.UpdateGetIt = cGenCharLocalLogic_UpdateGetIt,
};
