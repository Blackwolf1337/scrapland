/*
 * Logic/BankDirectorLogic.cpp of the game: the bank director, a generic character (cGenCharLocalLogic) whose
 * special action robs: it keeps a target with money in view and searches one in the character lists, the player
 * first when BankDirectorAgressive (include/scrap/logic.h). The update and the target test are shared with the
 * bank master and the bishop.
 */
#include "scrap/chartype.h"

cGenCharLocalLogic *__thiscall cBankDirectorLocalLogic_ctor(cGenCharLocalLogic *self, cEntity *parent) {
	cGenCharLocalLogic_ctor(self, parent, 0);
	self->base.vftable = CBANKDIRECTORLOCALLOGIC_VFTABLE;
	self->ignoreAlarm = 1;
	return self;
}

cGenCharLocalLogic *__thiscall cBankDirectorLocalLogic_Delete(cGenCharLocalLogic *self, byte flags) {
	self->base.vftable = CBANKDIRECTORLOCALLOGIC_VFTABLE;
	cGenCharLocalLogic_dtor(self);
	if (flags & 1) {
		operator_delete_sized(self, sizeof(*self));
	}
	return self;
}

cEntity *Logic_RichUsrInView(cGenCharLocalLogic *self, cEntity *usr) {
	vec3_t tmp;

	cEntity *p = self->base.parent;
	if (Logic_InView(p, usr->vftable->GetPos(usr, &tmp), g_GenCharStdFar, g_GenCharStdFovY, g_GenCharStdFovX, 1) &&
		g_pWorld->money > 0) {
		return usr;
	}
	return NULL;
}

cEntity *Logic_FindInCharLists(cEntity *p) {
	static const int lists[] = { WORLD_GENCHARS1, WORLD_GENCHARS2, WORLD_NURSES, WORLD_SENTINELS, WORLD_GEARS };

	for (size_t i = 0; i < sizeof(lists) / sizeof(lists[0]); i++) {
		cEntity *e = Logic_FindInView(p, World_CharList(lists[i]), g_GenCharStdFar, g_GenCharStdFovY,
			g_GenCharStdFovX, 1, NULL);
		if (e) {
			return e;
		}
	}
	return NULL;
}

/* vf31: 0 while the target has more than 25 of money, the special time has not run out and it is in view (also of
 * the bank master) */
int __thiscall cBankDirectorLocalLogic_LoseTarget(cGenCharLocalLogic *self) {
	vec3_t tmp;

	cEntity *t = self->base.target.target;
	if (!t || !t->vftable->IsType(t, TYEN_WALKCHAR)) {
		return 1;
	}
	int money = t == cWorld_GetUsrEntity(g_pWorld, 0) ? g_pWorld->money : WChar_Money(t);
	cEntity *p = self->base.parent;
	if (money <= 25 || self->specialTime > g_GenCharSpecialActionTime) {
		return 1;
	}
	if (Logic_InView(p, t->vftable->GetPos(t, &tmp), g_GenCharStdFar, g_GenCharStdFovY, g_GenCharStdFovX, 1)) {
		return 0;
	}
	return 1;
}

/* vf30: the next target when the search time ran out */
cEntity *__thiscall cBankDirectorLocalLogic_FindTarget(cGenCharLocalLogic *self) {
	if (!(0.0f > self->searchTime)) {
		return NULL;
	}
	self->searchTime = g_GenCharMinSpecialActionTime * 0.5f;
	cEntity *usr = ChType_Usr0();
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

/* vf17 (also of the bank master and the bishop) */
void __thiscall cBankDirectorLocalLogic_Update(cGenCharLocalLogic *self, float dt) {
	if (self->specialOn) {
		self->specialTime = dt + self->specialTime;
	}
	cGenCharLocalLogic_Update(self, dt);
}
