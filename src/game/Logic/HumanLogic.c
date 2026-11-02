/*
 * Logic/HumanLogic.cpp of the game: cHumanLocalLogic (include/scrap/logic.h), the logic of the type "Human". It
 * has no states of its own (the hit and update slots are empty); its C function makes the characters it finds
 * (not the player, the gears, sentinels and police) get away from the last human made.
 */
#include <string.h>

#include "scrap/chartype.h"

/* the C function of the human (cHumanLocalLogic.func): the character of that name gets away from the human */
void HumanLogic_Escape(cAbstractFile *params) {
	char name[0x80];

	cAbstractFile_ReadString(params, name, sizeof(name));
	cEntity *e = World_FindEntity(name);
	if (!e) {
		return;
	}
	cEntity *usr = ChType_Usr0();
	if (!usr || usr == e || !g_HumanEnt || g_HumanEnt == e) {
		return;
	}
	cAIChar *ai = e->vftable->GetAI(e);
	CHECK(ai);
	const char *type = Logic_TypeName(e);
	if (!strcmp(type, "Gear") || !strcmp(type, "Sentinel") || !strcmp(type, "Police")) {
		return;
	}
	ai->vftable->SetState(ai, 5, g_HumanEnt, (double)g_GenCharEscapeSpeed);
}

void *cHumanLocalLogic_ctor(void *mem, cEntity *parent) {
	cHumanLocalLogic *self = mem;

	cBaseLocalLogic_ctor(&self->base, parent);
	self->base.vftable = CHUMANLOCALLOGIC_VFTABLE;
	cPy_Function_ctor(&self->func);
	g_HumanEnt = self->base.parent;
	cPy_Function_SetNative(&self->func, (nativefunc_t)HumanLogic_Escape);
	return self;
}

void cHumanLocalLogic_dtor(cHumanLocalLogic *self) {
	self->base.vftable = CHUMANLOCALLOGIC_VFTABLE;
	cPy_Function_dtor(&self->func);
	cBaseLocalLogic_dtor(&self->base);
}

cLocalLogic *cHumanLocalLogic_Delete(cLocalLogic *base, byte flags) {
	cHumanLocalLogic *self = (cHumanLocalLogic *)base;
	cHumanLocalLogic_dtor(self);
	if (flags & 1) {
		operator_delete_sized(self, sizeof(*self));
	}
	return base;
}

/* the vtable of cHumanLocalLogic */
const cBaseLocalLogicVtbl cHumanLocalLogic_vftable = {
	.base.Delete = cHumanLocalLogic_Delete,
	.base.GetName = cBaseLocalLogic_GetName,
	.base.Vf02 = cLocalLogic_Vf02True,
	.base.VarI = cBaseLocalLogic_VarI,
	.base.VarF = cBaseLocalLogic_VarF,
	.base.VarS = cBaseLocalLogic_VarS,
	.base.VarV = cBaseLocalLogic_VarV,
	.UpdateBusy = cBaseLocalLogic_UpdateBusy,
	.Refresh = cBaseLocalLogic_Refresh,
	.Listens = cLocalLogic_ListensFalse,
	.OnHit = cLocalLogic_OnHitNop,
	.OnSeen = cLocalLogic_OnSeenNop,
	.OnAttack = cLocalLogic_OnAttackNop,
	.OnShot = cBaseLocalLogic_OnShot,
	.OnHammer = cLocalLogic_OnHammerNop,
	.GivesMoney = cBaseLocalLogic_Vf15,
	.OnAlarm = cLocalLogic_OnAlarmNop,
	.Update = cLocalLogic_UpdateNop,
	.SetOriginFrom = cBaseLocalLogic_SetOriginFrom,
	.SetState = cLocalLogic_SetStateTrue,
	.SetIdle = cBaseLocalLogic_SetIdle,
	.SetActionRadius = cBaseLocalLogic_SetActionRadius,
	.SetOrigin = cBaseLocalLogic_SetOrigin,
	.SetOriginAng = cBaseLocalLogic_SetOriginAng,
	.RestartState = cBaseLocalLogic_RestartState,
};
