/*
 * Logic/DesktopLogic.cpp of the game: the desktop (Betty's and Berto's chair), a generic character without a
 * target and vf09 0 (include/scrap/logic.h).
 */
#include "scrap/logic.h"

cDesktopLocalLogic *cDesktopLocalLogic_ctor(cDesktopLocalLogic *self, cEntity *parent) {
	cGenCharLocalLogic_ctor(self, parent, 0);
	self->base.vftable = CDESKTOPLOCALLOGIC_VFTABLE;
	return self;
}

cLocalLogic *cDesktopLocalLogic_Delete(cLocalLogic *elem, byte flags) {
	cDesktopLocalLogic *self = (cDesktopLocalLogic *)elem;
	self->base.vftable = CDESKTOPLOCALLOGIC_VFTABLE;
	cGenCharLocalLogic_dtor(self);
	if (flags & 1) {
		operator_delete_sized(self, sizeof(*self));
	}
	return elem;
}

/* the vtable of cDesktopLocalLogic */
const cGenCharLocalLogicVtbl cDesktopLocalLogic_vftable = {
	.base.base.Delete = cDesktopLocalLogic_Delete,
	.base.base.GetName = cBaseLocalLogic_GetName,
	.base.base.Vf02 = cLocalLogic_Vf02True,
	.base.base.VarI = cGenCharLocalLogic_VarI,
	.base.base.VarF = cBaseLocalLogic_VarF,
	.base.base.VarS = cGenCharLocalLogic_VarS,
	.base.base.VarV = cBaseLocalLogic_VarV,
	.base.UpdateBusy = cBaseLocalLogic_UpdateBusy,
	.base.Refresh = cBaseLocalLogic_Refresh,
	.base.Listens = cLocalLogic_ListensFalse,
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
