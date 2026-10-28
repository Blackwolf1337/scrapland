/*
 * Logic/BankMasterLogic.cpp of the game: the bank master, like the bank director (BankDirectorLogic.c) without the
 * aggressive mode; an attack makes it fight back (state 4) at random (include/scrap/logic.h).
 */
#include "scrap/chartype.h"

cGenCharLocalLogic *__thiscall cBankMasterLocalLogic_ctor(cGenCharLocalLogic *self, cEntity *parent) {
	cGenCharLocalLogic_ctor(self, parent, 0);
	self->base.vftable = CBANKMASTERLOCALLOGIC_VFTABLE;
	self->ignoreAlarm = 1;
	return self;
}

cGenCharLocalLogic *__thiscall cBankMasterLocalLogic_Delete(cGenCharLocalLogic *self, byte flags) {
	self->base.vftable = CBANKMASTERLOCALLOGIC_VFTABLE;
	cGenCharLocalLogic_dtor(self);
	if (flags & 1) {
		operator_delete_sized(self, sizeof(*self));
	}
	return self;
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
cEntity *__thiscall cBankMasterLocalLogic_FindTarget(cGenCharLocalLogic *self) {
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
void __thiscall cBankMasterLocalLogic_OnAttack(cGenCharLocalLogic *self, cEntity *attacker, float value) {
	(void)value;
	if (Logic_Chance(&self->base, attacker, 100.0f)) {
		self->base.vftable->SetState(self, 4, attacker);
	}
}
