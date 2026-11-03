/*
 * Logic/BertoLogic.cpp of the game: Berto, a functionary (cFunctionaryLocalLogic) at the desktop "BertoDesktop"
 * (include/scrap/logic.h). Berto_Default takes the desktop on its first update. BettyLogic.c is the same for Betty.
 */
#include <string.h>

#include "scrap/logic.h"
#include "scrap/objentity.h"

#define CBERTOLOCALLOGIC_VFTABLE (&cBertoLocalLogic_vftable.base.base.base)
static const cBertoLocalLogicVtbl cBertoLocalLogic_vftable;

cBertoLocalLogic *cBertoLocalLogic_ctor(cBertoLocalLogic *self, cEntity *parent) {
	cFunctionaryLocalLogic_ctor(&self->base, parent);
	self->base.base.base.vftable = CBERTOLOCALLOGIC_VFTABLE;
	self->isDefault = !strcmp(parent->name, "Berto_Default");
	return self;
}

static cLocalLogic *cBertoLocalLogic_Delete(cLocalLogic *elem, byte flags) {
	cBertoLocalLogic *self = (cBertoLocalLogic *)elem;
	self->base.base.base.vftable = CBERTOLOCALLOGIC_VFTABLE;
	cFunctionaryLocalLogic_dtor(&self->base);
	if (flags & 1) {
		operator_delete_sized(self, sizeof(*self));
	}
	return elem;
}

/* the desktop of the name linked, when the functionary has none (also of Betty) */
void Logic_LinkDesktop(cFunctionaryLocalLogic *self, const char *name) {
	if (!cFunctionaryLocalLogic_GetDesktop(self)) {
		cEntity *d = World_FindEntity(name);
		if (d) {
			cEntity_AddLink(d, &self->desktop);
		}
	}
}

/* vf40 */
static void cBertoLocalLogic_AssignDesktop(cLocalLogic *elem, float maxDist) {
	cBertoLocalLogic *self = (cBertoLocalLogic *)elem;
	(void)maxDist;
	Logic_LinkDesktop(&self->base, "BertoDesktop");
}

/* vf41 (also of Betty) */
void cBertoLocalLogic_ReleaseDesktop(cLocalLogic *elem) {
	cBertoLocalLogic *self = (cBertoLocalLogic *)elem;
	if (cFunctionaryLocalLogic_GetDesktop(&self->base)) {
		cFunctionaryLocalLogic_FreeDesktop(&self->base);
		CHECK(self->base.desktop.data);
		Nto1Node_Unlink(&self->base.desktop);
	}
}

/* vf17 (also of Betty): the default character takes its desktop */
void cBertoLocalLogic_Update(cLocalLogic *elem, float dt) {
	cBertoLocalLogic *self = (cBertoLocalLogic *)elem;
	if (self->isDefault && !cFunctionaryLocalLogic_GetDesktop(&self->base)) {
		Functionary_AssignDesktop(self, 0);
		self->isDefault = 0;
	}
	cFunctionaryLocalLogic_Update(&self->base.base.base.logic, dt);
}

/* vf38 (the callers use the returned out): the angles at the desktop, turned by a quarter */
static vec3_t *cBertoLocalLogic_GetWorkAng(cLocalLogic *elem, vec3_t *out) {
	cBertoLocalLogic *self = (cBertoLocalLogic *)elem;
	vec3_t tmp;

	CHECK(cFunctionaryLocalLogic_GetDesktop(&self->base));
	cEntity *d = cFunctionaryLocalLogic_GetDesktop(&self->base);
	const vec3_t *a = Obj_GetAng(d, &tmp);
	out->x = a->x - 0.0f;
	out->y = a->y - 1.57079637f;
	out->z = a->z - 0.0f;
	return out;
}

/* the vtable of cBertoLocalLogic */
static const cBertoLocalLogicVtbl cBertoLocalLogic_vftable = {
	.base.base.base.base.Delete = cBertoLocalLogic_Delete,
	.base.base.base.base.GetName = cBaseLocalLogic_GetName,
	.base.base.base.base.Vf02 = cFunctionaryLocalLogic_Vf02,
	.base.base.base.base.VarI = cFunctionaryLocalLogic_VarI,
	.base.base.base.base.VarF = cFunctionaryLocalLogic_VarF,
	.base.base.base.base.VarS = cFunctionaryLocalLogic_VarS,
	.base.base.base.base.VarV = cBaseLocalLogic_VarV,
	.base.base.base.UpdateBusy = cBaseLocalLogic_UpdateBusy,
	.base.base.base.Refresh = cBaseLocalLogic_Refresh,
	.base.base.base.Listens = cLocalLogic_ListensTrue,
	.base.base.base.OnHit = cGenCharLocalLogic_OnHit,
	.base.base.base.OnSeen = cGenCharLocalLogic_OnSeen,
	.base.base.base.OnAttack = cFunctionaryLocalLogic_OnRobbery,
	.base.base.base.OnShot = cBaseLocalLogic_OnShot,
	.base.base.base.OnHammer = cLocalLogic_OnHammerNop,
	.base.base.base.GivesMoney = cGenCharLocalLogic_GivesMoney,
	.base.base.base.OnAlarm = cGenCharLocalLogic_OnAlarm,
	.base.base.base.Update = cBertoLocalLogic_Update,
	.base.base.base.SetOriginFrom = cLocalLogic_SetOriginFromNop,
	.base.base.base.SetState = cFunctionaryLocalLogic_SetState,
	.base.base.base.SetIdle = cFunctionaryLocalLogic_SetIdle,
	.base.base.base.SetActionRadius = cBaseLocalLogic_SetActionRadius,
	.base.base.base.SetOrigin = cBaseLocalLogic_SetOrigin,
	.base.base.base.SetOriginAng = cBaseLocalLogic_SetOriginAng,
	.base.base.base.RestartState = cGenCharLocalLogic_RestartState,
	.base.base.CanAttack = cGenCharLocalLogic_CanAttack,
	.base.base.UpdateView = cFunctionaryLocalLogic_UpdateView,
	.base.base.ToRelax = cGenCharLocalLogic_ToRelax,
	.base.base.ToAlert = cGenCharLocalLogic_ToAlert,
	.base.base.Accuse = Logic_SetState2,
	.base.base.FindTarget = cLocalLogic_FindTargetNull,
	.base.base.TargetDone = cLocalLogic_TargetDoneTrue,
	.base.base.Work = cFunctionaryLocalLogic_Work,
	.base.base.Vf33 = cFunctionaryLocalLogic_Vf33,
	.base.base.GoTo = cGenCharLocalLogic_GoTo,
	.base.base.UpdateGetIt = cGenCharLocalLogic_UpdateGetIt,
	.base.CanTalk = cLocalLogic_CanTalkTrue,
	.base.StopWork = cFunctionaryLocalLogic_StopWork,
	.base.GetWorkAng = cBertoLocalLogic_GetWorkAng,
	.base.FastEscape = cLocalLogic_FastEscapeFalse,
	.base.AssignDesktop = cBertoLocalLogic_AssignDesktop,
	.base.ReleaseDesktop = cBertoLocalLogic_ReleaseDesktop,
	.Vf42 = cLocalLogic_Vf42Nop,
	.Vf43 = cLocalLogic_Vf43Nop,
	.Vf44 = cLocalLogic_Vf44Nop,
};
