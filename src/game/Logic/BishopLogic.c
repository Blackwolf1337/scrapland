/*
 * Logic/BishopLogic.cpp of the game: the bishop, a generic character that sells life: it is busy in SellLife, its
 * targets are the characters in view (no other bishop) when the special action is on, and on an alarm it goes to
 * its alarm state (include/scrap/logic.h). Its update is the one of the bank director (BankDirectorLogic.c).
 */
#include <string.h>

#include "scrap/chartype.h"

/* the AI slot the bishop asks */
typedef struct cLogicBishopAIVtbl {
	void *vf00_24[37];
	byte(*Vf94)(void *self);	/* 0x94 */
} cLogicBishopAIVtbl;
SD_OFFSET(cLogicBishopAIVtbl, Vf94, 0x94);

cBishopLocalLogic *cBishopLocalLogic_ctor(cBishopLocalLogic *self, cEntity *parent) {
	cGenCharLocalLogic_ctor(self, parent, 0);
	self->base.vftable = CBISHOPLOCALLOGIC_VFTABLE;
	self->alarmState = 4;
	return self;
}

cBishopLocalLogic *cBishopLocalLogic_Delete(cBishopLocalLogic *self, byte flags) {
	self->base.vftable = CBISHOPLOCALLOGIC_VFTABLE;
	cGenCharLocalLogic_dtor(self);
	if (flags & 1) {
		operator_delete_sized(self, sizeof(*self));
	}
	return self;
}

/* vf31: 0 to keep a character target that is idle when the AI asks */
byte cBishopLocalLogic_LoseTarget(cBishopLocalLogic *self) {
	cEntity *t = self->base.target.target;

	if (!t || !t->vftable->IsType(t, TYEN_WALKCHAR)) {
		return 1;
	}
	cEntity *p = self->base.parent;
	void *ai = p->vftable->GetAI(p);
	if (!(*(const cLogicBishopAIVtbl **)ai)->Vf94(ai)) {
		return 1;
	}
	cBaseLocalLogic *l = (cBaseLocalLogic *)Logic_GetLocalLogic(t);
	if (!l || (!l->idle && !l->alwaysLock)) {
		return 0;
	}
	return 1;
}

/* vf07 */
byte cBishopLocalLogic_UpdateBusy(cBishopLocalLogic *self) {
	if (!strncmp(cActCtrl_GetActionName(ChType_ActCtrl(self->base.parent), 0), "SellLife", 8)) {
		self->base.busy = 1;
		return self->base.busy;
	}
	byte r = cBaseLocalLogic_UpdateBusy(&self->base);
	self->base.busy = r;
	return r;
}

/* vf30: with the special action on (energy bar active, SpecialActionActive 2) */
cEntity *cBishopLocalLogic_FindTarget(cBishopLocalLogic *self) {
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
void cBishopLocalLogic_OnAlarm(cBishopLocalLogic *self, cEntity *target) {
	if (g_BishopAttackOnAlarm) {
		self->base.vftable->SetState(self, self->alarmState, target);
		return;
	}
	self->base.vftable->SetState(self, 2, target);
}
