/*
 * Logic/CrazyDealerLogic.cpp of the game: the crazy dealer, a generic character without a target of its own
 * (include/scrap/logic.h).
 */
#include "scrap/logic.h"

cCrazyDealerLocalLogic *__thiscall cCrazyDealerLocalLogic_ctor(cCrazyDealerLocalLogic *self, cEntity *parent) {
	cGenCharLocalLogic_ctor(self, parent, 0);
	self->base.vftable = CCRAZYDEALERLOCALLOGIC_VFTABLE;
	return self;
}

cCrazyDealerLocalLogic *__thiscall cCrazyDealerLocalLogic_Delete(cCrazyDealerLocalLogic *self, byte flags) {
	self->base.vftable = CCRAZYDEALERLOCALLOGIC_VFTABLE;
	cGenCharLocalLogic_dtor(self);
	if (flags & 1) {
		operator_delete_sized(self, sizeof(*self));
	}
	return self;
}

/* vf17 */
void __thiscall cCrazyDealerLocalLogic_Update(cCrazyDealerLocalLogic *self, float dt) {
	cGenCharLocalLogic_Update(self, dt);
}
