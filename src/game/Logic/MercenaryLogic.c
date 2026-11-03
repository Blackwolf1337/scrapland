/*
 * Logic/MercenaryLogic.cpp of the game: the mercenaries (the types "Mercenary..."), a GenChar logic without a
 * special attack target (vf30 NULL, vf31 1); the update is the one of the GenChars.
 */
#include "scrap/logic.h"

#define CMERCENARYLOCALLOGIC_VFTABLE (&cMercenaryLocalLogic_vftable.base)
static const cGenCharLocalLogicVtbl cMercenaryLocalLogic_vftable;
#define CMERC_VT(self) ((self)->base.vftable)

cMercenaryLocalLogic *cMercenaryLocalLogic_ctor(cMercenaryLocalLogic *self, cEntity *parent) {
	cGenCharLocalLogic_ctor(self, parent, 0);
	CMERC_VT(self) = CMERCENARYLOCALLOGIC_VFTABLE;
	return self;
}

static cLocalLogic *cMercenaryLocalLogic_Delete(cLocalLogic *base, byte flags) {
	cMercenaryLocalLogic *self = (cMercenaryLocalLogic *)base;
	CMERC_VT(self) = CMERCENARYLOCALLOGIC_VFTABLE;
	cGenCharLocalLogic_dtor(self);
	if (flags & 1) {
		operator_delete_sized(self, sizeof(*self));
	}
	return base;
}

/* the vtable of cMercenaryLocalLogic */
static const cGenCharLocalLogicVtbl cMercenaryLocalLogic_vftable = {
	.base.base.Delete = cMercenaryLocalLogic_Delete,
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
	.base.Update = cGenCharLocalLogic_Update,
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
	.Vf33 = cLocalLogic_Vf33Nop,
	.GoTo = cGenCharLocalLogic_GoTo,
	.UpdateGetIt = cGenCharLocalLogic_UpdateGetIt,
};
