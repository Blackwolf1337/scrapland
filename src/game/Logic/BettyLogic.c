/*
 * Logic/BettyLogic.cpp of the game: Betty, a functionary at the desktop "BettyDesktop" (include/scrap/logic.h); the
 * release, the update and the slots without code are the ones of Berto (BertoLogic.c).
 */
#include <string.h>

#include "scrap/logic.h"
#include "scrap/objentity.h"

#define CBETTYLOCALLOGIC_VFTABLE (&cBettyLocalLogic_vftable.base.base)
static const cFunctionaryLocalLogicVtbl cBettyLocalLogic_vftable;

cBettyLocalLogic *cBettyLocalLogic_ctor(cBettyLocalLogic *self, cEntity *parent) {
	cFunctionaryLocalLogic_ctor(&self->base, parent);
	self->base.base.base.vftable = CBETTYLOCALLOGIC_VFTABLE;
	self->isDefault = !strcmp(parent->name, "Betty_Default");
	return self;
}

static void cBettyLocalLogic_Free(cLocalLogic *elem) {
	cBettyLocalLogic *self = (cBettyLocalLogic *)elem;
	cFunctionaryLocalLogic_dtor(&self->base);
	operator_delete(self);
}

static void cBettyLocalLogic_AssignDesktop(cLocalLogic *elem, float maxDist) {
	cBettyLocalLogic *self = (cBettyLocalLogic *)elem;
	(void)maxDist;
	Logic_LinkDesktop(&self->base, "BettyDesktop");
}

/* (the callers use the returned out): the angles at the desktop, turned by a quarter the other way */
static vec3_t *cBettyLocalLogic_GetWorkAng(cLocalLogic *elem, vec3_t *out) {
	cBettyLocalLogic *self = (cBettyLocalLogic *)elem;
	vec3_t tmp;

	CHECK(cFunctionaryLocalLogic_GetDesktop(&self->base));
	cEntity *d = cFunctionaryLocalLogic_GetDesktop(&self->base);
	const vec3_t *a = Obj_GetAng(d, &tmp);
	out->x = a->x + 0.0f;
	out->y = a->y + 1.57079637f;
	out->z = a->z + 0.0f;
	return out;
}

/* the vtable of cBettyLocalLogic */
static const cFunctionaryLocalLogicVtbl cBettyLocalLogic_vftable = {
	.base.base.base.Free = cBettyLocalLogic_Free,
	.base.base.base.GetName = cBaseLocalLogic_GetName,
	.base.base.base.AcceptsArrow = cFunctionaryLocalLogic_AcceptsArrow,
	.base.base.base.VarI = cFunctionaryLocalLogic_VarI,
	.base.base.base.VarF = cFunctionaryLocalLogic_VarF,
	.base.base.base.VarS = cFunctionaryLocalLogic_VarS,
	.base.base.base.VarV = cBaseLocalLogic_VarV,
	.base.base.UpdateBusy = cBaseLocalLogic_UpdateBusy,
	.base.base.Refresh = cBaseLocalLogic_Refresh,
	.base.base.Listens = cLocalLogic_ListensTrue,
	.base.base.OnHit = cGenCharLocalLogic_OnHit,
	.base.base.OnSeen = cGenCharLocalLogic_OnSeen,
	.base.base.OnAttack = cFunctionaryLocalLogic_OnRobbery,
	.base.base.OnShot = cBaseLocalLogic_OnShot,
	.base.base.OnHammer = cLocalLogic_OnHammerNop,
	.base.base.GivesMoney = cGenCharLocalLogic_GivesMoney,
	.base.base.OnAlarm = cGenCharLocalLogic_OnAlarm,
	.base.base.Update = cBertoLocalLogic_Update,
	.base.base.SetOriginFrom = cLocalLogic_SetOriginFromNop,
	.base.base.SetState = cFunctionaryLocalLogic_SetState,
	.base.base.SetIdle = cFunctionaryLocalLogic_SetIdle,
	.base.base.SetActionRadius = cBaseLocalLogic_SetActionRadius,
	.base.base.SetOrigin = cBaseLocalLogic_SetOrigin,
	.base.base.SetOriginAng = cBaseLocalLogic_SetOriginAng,
	.base.base.RestartState = cGenCharLocalLogic_RestartState,
	.base.CanAttack = cGenCharLocalLogic_CanAttack,
	.base.UpdateView = cFunctionaryLocalLogic_UpdateView,
	.base.ToRelax = cGenCharLocalLogic_ToRelax,
	.base.ToAlert = cGenCharLocalLogic_ToAlert,
	.base.Accuse = Logic_SetState2,
	.base.FindTarget = cLocalLogic_FindTargetNull,
	.base.TargetDone = cLocalLogic_TargetDoneTrue,
	.base.Work = cFunctionaryLocalLogic_Work,
	.base.BreakWork = cFunctionaryLocalLogic_BreakWork,
	.base.GoTo = cGenCharLocalLogic_GoTo,
	.base.UpdateGetIt = cGenCharLocalLogic_UpdateGetIt,
	.CanTalk = cLocalLogic_CanTalkTrue,
	.StopWork = cFunctionaryLocalLogic_StopWork,
	.GetWorkAng = cBettyLocalLogic_GetWorkAng,
	.FastEscape = cLocalLogic_FastEscapeFalse,
	.AssignDesktop = cBettyLocalLogic_AssignDesktop,
	.ReleaseDesktop = cBertoLocalLogic_ReleaseDesktop,
};
