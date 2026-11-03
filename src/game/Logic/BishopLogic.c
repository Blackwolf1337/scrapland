/*
 * Logic/BishopLogic.cpp of the game: the bishop, a generic character that sells life: it is busy in SellLife, its
 * targets are the characters in view (no other bishop) when the special action is on, and on an alarm it goes to
 * its alarm state (include/scrap/logic.h). Its update is the one of the bank director (BankDirectorLogic.c).
 */
#include <string.h>

#include "scrap/chartype.h"

#define CBISHOPLOCALLOGIC_VFTABLE (&cBishopLocalLogic_vftable.base)
static const cGenCharLocalLogicVtbl cBishopLocalLogic_vftable;

/* the AI slot the bishop asks */
typedef struct cLogicBishopAIVtbl {
	void *vf00_24[37];
	byte(*Vf94)(void *self);	/* 0x94 */
} cLogicBishopAIVtbl;

/* calls through the slots of cLogicBishopAIVtbl */
static inline byte BishopAI_Vf94(void *self) {
	return VT(cLogicBishopAIVtbl, self)->Vf94(self);
}

cBishopLocalLogic *cBishopLocalLogic_ctor(cBishopLocalLogic *self, cEntity *parent) {
	cGenCharLocalLogic_ctor(self, parent, 0);
	self->base.vftable = CBISHOPLOCALLOGIC_VFTABLE;
	self->alarmState = 4;
	return self;
}

static cLocalLogic *cBishopLocalLogic_Delete(cLocalLogic *elem, byte flags) {
	cBishopLocalLogic *self = (cBishopLocalLogic *)elem;
	self->base.vftable = CBISHOPLOCALLOGIC_VFTABLE;
	cGenCharLocalLogic_dtor(self);
	if (flags & 1) {
		operator_delete_sized(self, sizeof(*self));
	}
	return elem;
}

/* vf31: 0 to keep a character target that is idle when the AI asks */
static byte cBishopLocalLogic_LoseTarget(cLocalLogic *elem) {
	cBishopLocalLogic *self = (cBishopLocalLogic *)elem;
	cEntity *t = self->base.target.target;

	if (!t || !Entity_IsType(t, TYEN_WALKCHAR)) {
		return 1;
	}
	cEntity *p = self->base.parent;
	void *ai = Entity_GetAI(p);
	if (!BishopAI_Vf94(ai)) {
		return 1;
	}
	cBaseLocalLogic *l = (cBaseLocalLogic *)Logic_GetLocalLogic(t);
	if (!l || (!l->idle && !l->alwaysLock)) {
		return 0;
	}
	return 1;
}

/* vf07 */
static byte cBishopLocalLogic_UpdateBusy(cLocalLogic *elem) {
	cBishopLocalLogic *self = (cBishopLocalLogic *)elem;
	if (!strncmp(cActCtrl_GetActionName(ChType_ActCtrl(self->base.parent), 0), "SellLife", 8)) {
		self->base.busy = 1;
		return self->base.busy;
	}
	byte r = cBaseLocalLogic_UpdateBusy(&self->base.logic);
	self->base.busy = r;
	return r;
}

/* vf30: with the special action on (energy bar active, SpecialActionActive 2) */
static cEntity *cBishopLocalLogic_FindTarget(cLocalLogic *elem) {
	cBishopLocalLogic *self = (cBishopLocalLogic *)elem;
	cEntityListData **const lists[] = { &g_pWorld->logic.nurses.list, &g_pWorld->logic.aiCtrl.sentinels, &g_pWorld->logic.aiCtrl.gears };

	if (g_EnergyBarActive != 1 || g_SpecialActionActive != 2) {
		return NULL;
	}
	if (!(0.0f > self->searchTime)) {
		return NULL;
	}
	self->searchTime = g_GenCharMinSpecialActionTime * 0.5f;
	if (!cWorld_GetUsrEntity(g_pWorld, 0)) {
		return NULL;
	}
	cEntity *p = self->base.parent;
	cEntity *e = Logic_FindInView(p, g_pWorld->logic.genChars.genChars, g_GenCharStdFar, g_GenCharStdFovY,
		g_GenCharStdFovX, 1, NULL);
	if (e && strcmp(ChType_ActClass(ChType_ActCtrl(e))->name, "Bishop")) {
		return e;
	}
	for (size_t i = 0; i < sizeof(lists) / sizeof(lists[0]); i++) {
		e = Logic_FindInView(p, *lists[i], g_GenCharStdFar, g_GenCharStdFovY, g_GenCharStdFovX, 1,
			NULL);
		if (e) {
			return e;
		}
	}
	return NULL;
}

/* vf29 (0x74), the alarm: its alarm state or 2 against target */
static void cBishopLocalLogic_OnAlarm(cLocalLogic *elem, cEntity *target) {
	cBishopLocalLogic *self = (cBishopLocalLogic *)elem;
	if (g_BishopAttackOnAlarm) {
		Logic_SetState(self, self->alarmState, target);
		return;
	}
	Logic_SetState(self, GENCHAR_ACCUSE, target);
}

/* the vtable of cBishopLocalLogic */
static const cGenCharLocalLogicVtbl cBishopLocalLogic_vftable = {
	.base.base.Delete = cBishopLocalLogic_Delete,
	.base.base.GetName = cBaseLocalLogic_GetName,
	.base.base.Vf02 = cLocalLogic_Vf02True,
	.base.base.VarI = cGenCharLocalLogic_VarI,
	.base.base.VarF = cBaseLocalLogic_VarF,
	.base.base.VarS = cGenCharLocalLogic_VarS,
	.base.base.VarV = cBaseLocalLogic_VarV,
	.base.UpdateBusy = cBishopLocalLogic_UpdateBusy,
	.base.Refresh = cBaseLocalLogic_Refresh,
	.base.Listens = cLocalLogic_ListensTrue,
	.base.OnHit = cGenCharLocalLogic_OnHit,
	.base.OnSeen = cGenCharLocalLogic_OnSeen,
	.base.OnAttack = cGenCharLocalLogic_OnRobbery,
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
	.Accuse = cBishopLocalLogic_OnAlarm,
	.FindTarget = cBishopLocalLogic_FindTarget,
	.TargetDone = cBishopLocalLogic_LoseTarget,
	.Work = cLocalLogic_WorkNop,
	.Vf33 = cLocalLogic_Vf33Nop,
	.GoTo = cGenCharLocalLogic_GoTo,
	.UpdateGetIt = cGenCharLocalLogic_UpdateGetIt,
};
