/*
 * Logic/HumphreyLogic.cpp of the game: Humphrey, a functionary (cFunctionaryLocalLogic, include/scrap/logic.h) at
 * the desktop "HumphreyDesktop". The character "Humphrey_Default" takes its desktop at the first update.
 */
#include <string.h>

#include "scrap/chartype.h"

#define CHUMPHREYLOCALLOGIC_VFTABLE (&cHumphreyLocalLogic_vftable.base.base)
static const cFunctionaryLocalLogicVtbl cHumphreyLocalLogic_vftable;

#define Humphrey_Vtbl(self) Functionary_Vtbl(self)

cHumphreyLocalLogic *cHumphreyLocalLogic_ctor(cHumphreyLocalLogic *self, cEntity *parent) {
	cFunctionaryLocalLogic_ctor(&self->base, parent);
	self->base.base.base.vftable = CHUMPHREYLOCALLOGIC_VFTABLE;
	self->isDefault = !strcmp(parent->name, "Humphrey_Default");
	return self;
}

static cLocalLogic *cHumphreyLocalLogic_Delete(cLocalLogic *elem, byte flags) {
	cHumphreyLocalLogic *self = (cHumphreyLocalLogic *)elem;
	self->base.base.base.vftable = CHUMPHREYLOCALLOGIC_VFTABLE;
	cFunctionaryLocalLogic_dtor(&self->base);
	if (flags & 1) {
		operator_delete_sized(self, sizeof(*self));
	}
	return elem;
}

/* vf40 */
static void cHumphreyLocalLogic_AssignDesktop(cLocalLogic *elem, float maxDist) {
	cHumphreyLocalLogic *self = (cHumphreyLocalLogic *)elem;
	(void)maxDist;
	if (cFunctionaryLocalLogic_GetDesktop(&self->base)) {
		return;
	}
	cEntity *d = World_FindEntity("HumphreyDesktop");
	if (d) {
		cEntity_AddLink(d, &self->base.desktop);
	}
}

/* vf41 */
static void cHumphreyLocalLogic_ReleaseDesktop(cLocalLogic *elem) {
	cHumphreyLocalLogic *self = (cHumphreyLocalLogic *)elem;
	if (!cFunctionaryLocalLogic_GetDesktop(&self->base)) {
		return;
	}
	cFunctionaryLocalLogic_FreeDesktop(&self->base);
	CHECK(self->base.desktop.data);
	Nto1Node_Unlink(&self->base.desktop);
}

/* vf17: the default Humphrey takes its desktop once */
static void cHumphreyLocalLogic_Update(cLocalLogic *elem, float dt) {
	cHumphreyLocalLogic *self = (cHumphreyLocalLogic *)elem;
	if (self->isDefault && !cFunctionaryLocalLogic_GetDesktop(&self->base)) {
		Humphrey_Vtbl(self)->AssignDesktop(&self->base.base.base.logic, 0.0f);
		self->isDefault = 0;
	}
	cFunctionaryLocalLogic_Update(&self->base.base.base.logic, dt);
}

/* vf38: the angles of the desktop */
static vec3_t *cHumphreyLocalLogic_GetWorkAng(cLocalLogic *elem, vec3_t *out) {
	cHumphreyLocalLogic *self = (cHumphreyLocalLogic *)elem;
	CHECK(cFunctionaryLocalLogic_GetDesktop(&self->base));
	cEntity *d = cFunctionaryLocalLogic_GetDesktop(&self->base);
	Obj_Vtbl(d)->GetAng(d, out);
	return out;
}

/* the vtable of cHumphreyLocalLogic */
static const cFunctionaryLocalLogicVtbl cHumphreyLocalLogic_vftable = {
	.base.base.base.Delete = cHumphreyLocalLogic_Delete,
	.base.base.base.GetName = cBaseLocalLogic_GetName,
	.base.base.base.Vf02 = cFunctionaryLocalLogic_Vf02,
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
	.base.base.Update = cHumphreyLocalLogic_Update,
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
	.base.Vf33 = cFunctionaryLocalLogic_Vf33,
	.base.GoTo = cGenCharLocalLogic_GoTo,
	.base.UpdateGetIt = cGenCharLocalLogic_UpdateGetIt,
	.CanTalk = cLocalLogic_CanTalkTrue,
	.StopWork = cFunctionaryLocalLogic_StopWork,
	.GetWorkAng = cHumphreyLocalLogic_GetWorkAng,
	.FastEscape = cLocalLogic_FastEscapeFalse,
	.AssignDesktop = cHumphreyLocalLogic_AssignDesktop,
	.ReleaseDesktop = cHumphreyLocalLogic_ReleaseDesktop,
};
