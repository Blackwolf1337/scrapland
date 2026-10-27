/*
 * Logic/CrazyDealerLogic.cpp of the game: the crazy dealer, a generic character without a target of its own
 * (include/scrap/logic.h).
 */
#include "scrap/logic.h"

cGenCharLocalLogic *__thiscall cCrazyDealerLocalLogic_ctor(cGenCharLocalLogic *self, cEntity *parent) {
	cGenCharLocalLogic_ctor(self, parent, 0);
	self->base.vftable = Game_Ptr(CCRAZYDEALERLOCALLOGIC_VFTABLE);
	return self;
}

cGenCharLocalLogic *__thiscall cCrazyDealerLocalLogic_Delete(cGenCharLocalLogic *self, byte flags) {
	self->base.vftable = Game_Ptr(CCRAZYDEALERLOCALLOGIC_VFTABLE);
	cGenCharLocalLogic_dtor(self);
	if (flags & 1) {
		operator_delete_sized(self, sizeof(*self));
	}
	return self;
}

/* vf17 */
void __thiscall cCrazyDealerLocalLogic_Update(cGenCharLocalLogic *self, float dt) {
	cGenCharLocalLogic_Update(self, dt);
}
