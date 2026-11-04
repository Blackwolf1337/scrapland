/*
 * Logic/CrazyDealerLogic.cpp of the game: the crazy dealer, a generic character without a target of its own
 * (include/scrap/logic.h).
 */
#include "scrap/logic.h"

#define CCRAZYDEALERLOCALLOGIC_VFTABLE (&cCrazyDealerLocalLogic_vftable.base)
static const cGenCharLocalLogicVtbl cCrazyDealerLocalLogic_vftable;

cCrazyDealerLocalLogic *cCrazyDealerLocalLogic_ctor(cCrazyDealerLocalLogic *self, cEntity *parent) {
	cGenCharLocalLogic_ctor(self, parent, 0);
	self->base.vftable = CCRAZYDEALERLOCALLOGIC_VFTABLE;
	return self;
}

static void cCrazyDealerLocalLogic_Free(cLocalLogic *elem) {
	cCrazyDealerLocalLogic *self = (cCrazyDealerLocalLogic *)elem;
	self->base.vftable = CCRAZYDEALERLOCALLOGIC_VFTABLE;
	cGenCharLocalLogic_dtor(self);
	operator_delete(self);
}

/* vf17 */
static void cCrazyDealerLocalLogic_Update(cLocalLogic *base, float dt) {
	cCrazyDealerLocalLogic *self = (cCrazyDealerLocalLogic *)base;
	cGenCharLocalLogic_Update(&self->base.logic, dt);
}

/* the vtable of cCrazyDealerLocalLogic */
static const cGenCharLocalLogicVtbl cCrazyDealerLocalLogic_vftable = {
	.base.base.Free = cCrazyDealerLocalLogic_Free,
	.base.base.GetName = cBaseLocalLogic_GetName,
	.base.base.AcceptsArrow = cLocalLogic_AcceptsArrowTrue,
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
	.base.Update = cCrazyDealerLocalLogic_Update,
	.base.SetOriginFrom = cBaseLocalLogic_SetOriginFrom,
	.base.SetState = cGenCharLocalLogic_SetState,
	.base.SetIdle = cBaseLocalLogic_SetIdle,
	.base.SetActionRadius = cBaseLocalLogic_SetActionRadius,
	.base.SetOrigin = cBaseLocalLogic_SetOrigin,
	.base.SetOriginAng = cBaseLocalLogic_SetOriginAng,
	.base.RestartState = cGenCharLocalLogic_RestartState,
	.CanAttack = cGenCharLocalLogic_CanAttack,
	.UpdateView = cGenCharLocalLogic_UpdateView,
	.ToRelax = cGenCharLocalLogic_ToRelax,
	.ToAlert = cGenCharLocalLogic_ToAlert,
	.Accuse = Logic_SetState2,
	.FindTarget = cLocalLogic_FindTargetNull,
	.TargetDone = cLocalLogic_TargetDoneTrue,
	.Work = cLocalLogic_WorkNop,
	.BreakWork = cLocalLogic_BreakWorkNop,
	.GoTo = cGenCharLocalLogic_GoTo,
	.UpdateGetIt = cGenCharLocalLogic_UpdateGetIt,
};
