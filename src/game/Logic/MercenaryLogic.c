/*
 * Logic/MercenaryLogic.cpp of the game: the mercenaries (the types "Mercenary..."), a GenChar logic without a
 * special attack target (vf30 NULL, vf31 1); the update is the one of the GenChars.
 */
#include "scrap/logic.h"

#define CMERC_VT(self) ((self)->base.vftable)

void *__thiscall cMercenaryLocalLogic_ctor(void *mem, cEntity *parent) {
	cGenCharLocalLogic *self = mem;

	cGenCharLocalLogic_ctor(self, parent, 0);
	CMERC_VT(self) = Game_Ptr(CMERCENARYLOCALLOGIC_VFTABLE);
	return self;
}

cGenCharLocalLogic *__thiscall cMercenaryLocalLogic_Delete(cGenCharLocalLogic *self, byte flags) {
	CMERC_VT(self) = Game_Ptr(CMERCENARYLOCALLOGIC_VFTABLE);
	cGenCharLocalLogic_dtor(self);
	if (flags & 1) {
		operator_delete_sized(self, sizeof(*self));
	}
	return self;
}
