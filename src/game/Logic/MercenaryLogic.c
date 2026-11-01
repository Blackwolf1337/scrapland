/*
 * Logic/MercenaryLogic.cpp of the game: the mercenaries (the types "Mercenary..."), a GenChar logic without a
 * special attack target (vf30 NULL, vf31 1); the update is the one of the GenChars.
 */
#include "scrap/logic.h"

#define CMERC_VT(self) ((self)->base.vftable)

cMercenaryLocalLogic *cMercenaryLocalLogic_ctor(cMercenaryLocalLogic *self, cEntity *parent) {
	cGenCharLocalLogic_ctor(self, parent, 0);
	CMERC_VT(self) = CMERCENARYLOCALLOGIC_VFTABLE;
	return self;
}

cMercenaryLocalLogic *cMercenaryLocalLogic_Delete(cMercenaryLocalLogic *self, byte flags) {
	CMERC_VT(self) = CMERCENARYLOCALLOGIC_VFTABLE;
	cGenCharLocalLogic_dtor(self);
	if (flags & 1) {
		operator_delete_sized(self, sizeof(*self));
	}
	return self;
}
