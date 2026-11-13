/*
 * Logic/BankDirectorLogic.cpp of the game: the bank director, a generic character (cGenCharLocalLogic) whose
 * special action robs: it keeps a target with money in view and searches one in the character lists, the player
 * first when BankDirectorAgressive (include/scrap/logic.h). The update and the target test are shared with the
 * bank master and the bishop.
 */
#include "scrap/chartype.h"
#include "scrap/staticinit.h"
#include "scrap/players.h"

#define CBANKDIRECTORLOCALLOGIC_VFTABLE (&cBankDirectorLocalLogic_vftable.base)
static const cGenCharLocalLogicVtbl cBankDirectorLocalLogic_vftable;

cBankDirectorLocalLogic *cBankDirectorLocalLogic_ctor(cBankDirectorLocalLogic *self, cEntity *parent) {
	cGenCharLocalLogic_ctor(self, parent, 0);
	self->base.vftable = CBANKDIRECTORLOCALLOGIC_VFTABLE;
	self->ignoreAlarm = 1;
	return self;
}

static void cBankDirectorLocalLogic_Free(cLocalLogic *elem) {
	cBankDirectorLocalLogic *self = (cBankDirectorLocalLogic *)elem;
	cGenCharLocalLogic_dtor(self);
	operator_delete(self);
}

cEntity *Logic_RichUsrInView(cGenCharLocalLogic *self, cEntity *usr) {
	vec3_t tmp;

	cEntity *p = self->base.parent;
	if (Logic_InView(p, Entity_GetPos(usr, &tmp), g_GenCharStdFar, g_GenCharStdFovY, g_GenCharStdFovX, 1) &&
		g_pWorld->money > 0) {
		return usr;
	}
	return NULL;
}

cEntity *Logic_FindInCharLists(cEntity *p) {
	cEntityListData **const lists[] = { &g_pWorld->logic.genChars.genChars, &g_pWorld->logic.genChars.police, &g_pWorld->logic.nurses.list, &g_pWorld->logic.aiCtrl.sentinels, &g_pWorld->logic.aiCtrl.gears };

	for (size_t i = 0; i < sizeof(lists) / sizeof(lists[0]); i++) {
		cEntity *e = Logic_FindInView(p, *lists[i], g_GenCharStdFar, g_GenCharStdFovY,
			g_GenCharStdFovX, 1, NULL);
		if (e) {
			return e;
		}
	}
	return NULL;
}

/* 0 while the target has more than 25 of money, the special time has not run out and it is in view (also of
 * the bank master) */
byte cBankDirectorLocalLogic_LoseTarget(cLocalLogic *elem) {
	cBankDirectorLocalLogic *self = (cBankDirectorLocalLogic *)elem;
	vec3_t tmp;

	cEntity *t = self->base.target.target;
	if (!t || !Entity_IsType(t, TYEN_WALKCHAR)) {
		return 1;
	}
	int money = Players_Is(t) ? g_pWorld->money : WChar(t)->money;
	cEntity *p = self->base.parent;
	if (money <= 25 || self->specialTime > g_GenCharSpecialActionTime) {
		return 1;
	}
	if (Logic_InView(p, Entity_GetPos(t, &tmp), g_GenCharStdFar, g_GenCharStdFovY, g_GenCharStdFovX, 1)) {
		return 0;
	}
	return 1;
}

/* the next target when the search time ran out */
static cEntity *cBankDirectorLocalLogic_FindTarget(cLocalLogic *elem) {
	cBankDirectorLocalLogic *self = (cBankDirectorLocalLogic *)elem;
	if (!(0.0f > self->searchTime)) {
		return NULL;
	}
	self->searchTime = g_GenCharMinSpecialActionTime * 0.5f;
	cEntity *usr = Players_Nearest(&self->base.parent->pos);
	if (!usr) {
		return NULL;
	}
	if (g_BankDirectorAgressive) {
		self->searchTime = 1.0f;
		if (Logic_RichUsrInView(self, usr)) {
			return usr;
		}
	}
	cEntity *e = Logic_FindInCharLists(self->base.parent);
	if (e) {
		return e;
	}
	if (!g_BankDirectorAgressive) {
		return Logic_RichUsrInView(self, usr);
	}
	return NULL;
}

/* (also of the bank master and the bishop) */
void cBankDirectorLocalLogic_Update(cLocalLogic *base, float dt) {
	cBankDirectorLocalLogic *self = (cBankDirectorLocalLogic *)base;
	if (self->specialOn) {
		self->specialTime = dt + self->specialTime;
	}
	cGenCharLocalLogic_Update(&self->base.logic, dt);
}

/* the vtable of cBankDirectorLocalLogic */
static const cGenCharLocalLogicVtbl cBankDirectorLocalLogic_vftable = {
	.base.base.Free = cBankDirectorLocalLogic_Free,
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
	.FindTarget = cBankDirectorLocalLogic_FindTarget,
	.TargetDone = cBankDirectorLocalLogic_LoseTarget,
	.Work = cLocalLogic_WorkNop,
	.BreakWork = cLocalLogic_BreakWorkNop,
	.GoTo = cGenCharLocalLogic_GoTo,
	.UpdateGetIt = cGenCharLocalLogic_UpdateGetIt,
};

cGVar GVar_GenCharSpecialActionTime;

/* the dynamic initializers of the globals here, run in the order of src/standalone/crtxc.inc */
INIT_GVAR_FLOAT(Init_GVar_GenCharSpecialActionTime, &GVar_GenCharSpecialActionTime, "GenCharSpecialActionTime", 0, "GenChar special action time", 15.0f)
