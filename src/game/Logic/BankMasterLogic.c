/*
 * Logic/BankMasterLogic.cpp of the game: the bank master, like the bank director (BankDirectorLogic.c) without the
 * aggressive mode; an attack makes it fight back (state 4) at random (include/scrap/logic.h).
 */
#include "scrap/chartype.h"

#define CBANKMASTERLOCALLOGIC_VFTABLE (&cBankMasterLocalLogic_vftable.base)
static const cGenCharLocalLogicVtbl cBankMasterLocalLogic_vftable;

cBankMasterLocalLogic *cBankMasterLocalLogic_ctor(cBankMasterLocalLogic *self, cEntity *parent) {
	cGenCharLocalLogic_ctor(self, parent, 0);
	self->base.vftable = CBANKMASTERLOCALLOGIC_VFTABLE;
	self->ignoreAlarm = 1;
	return self;
}

static cLocalLogic *cBankMasterLocalLogic_Delete(cLocalLogic *elem, byte flags) {
	cBankMasterLocalLogic *self = (cBankMasterLocalLogic *)elem;
	self->base.vftable = CBANKMASTERLOCALLOGIC_VFTABLE;
	cGenCharLocalLogic_dtor(self);
	if (flags & 1) {
		operator_delete_sized(self, sizeof(*self));
	}
	return elem;
}

byte Logic_Chance(cBaseLocalLogic *l, cEntity *attacker, float chance) {
	if (l->idle || l->alwaysLock) {
		cEntity *usr = ChType_Usr0();
		if (usr != attacker || l->busy) {
			return 0;
		}
	}
	double r = (double)(crt_rand() & 0x7fff) * (1.0 / 32767.0) * 100.0 + 0.0;
	return chance > (float)r;
}

/* vf30 */
static cEntity *cBankMasterLocalLogic_FindTarget(cLocalLogic *elem) {
	cBankMasterLocalLogic *self = (cBankMasterLocalLogic *)elem;
	if (!(0.0f > self->searchTime)) {
		return NULL;
	}
	self->searchTime = g_GenCharMinSpecialActionTime * 0.5f;
	cEntity *usr = ChType_Usr0();
	if (!usr) {
		return NULL;
	}
	cEntity *e = Logic_FindInCharLists(self->base.parent);
	if (e) {
		return e;
	}
	return Logic_RichUsrInView(self, usr);
}

/* vf12 (0x30), the attack of a character: state 4 against it */
static void cBankMasterLocalLogic_OnAttack(cLocalLogic *elem, cEntity *attacker, float value) {
	cBankMasterLocalLogic *self = (cBankMasterLocalLogic *)elem;
	(void)value;
	if (Logic_Chance(&self->base, attacker, 100.0f)) {
		Logic_SetState(self, GENCHAR_ATTACK, attacker);
	}
}

/* the vtable of cBankMasterLocalLogic */
static const cGenCharLocalLogicVtbl cBankMasterLocalLogic_vftable = {
	.base.base.Delete = cBankMasterLocalLogic_Delete,
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
	.base.OnAttack = cBankMasterLocalLogic_OnAttack,
	.base.OnShot = cBaseLocalLogic_OnShot,
	.base.OnHammer = cLocalLogic_OnHammerNop,
	.base.GivesMoney = cGenCharLocalLogic_GivesMoney,
	.base.OnAlarm = cGenCharLocalLogic_OnAlarm,
	.base.Update = cBankDirectorLocalLogic_Update,
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
	.FindTarget = cBankMasterLocalLogic_FindTarget,
	.TargetDone = cBankDirectorLocalLogic_LoseTarget,
	.Work = cLocalLogic_WorkNop,
	.BreakWork = cLocalLogic_BreakWorkNop,
	.GoTo = cGenCharLocalLogic_GoTo,
	.UpdateGetIt = cGenCharLocalLogic_UpdateGetIt,
};
