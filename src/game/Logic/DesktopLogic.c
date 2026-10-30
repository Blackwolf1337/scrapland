/*
 * Logic/DesktopLogic.cpp of the game: the desktop (Betty's and Berto's chair), a generic character without a
 * target and vf09 0 (include/scrap/logic.h).
 */
#include "scrap/logic.h"

cDesktopLocalLogic *__thiscall cDesktopLocalLogic_ctor(cDesktopLocalLogic *self, cEntity *parent) {
	cGenCharLocalLogic_ctor(self, parent, 0);
	self->base.vftable = CDESKTOPLOCALLOGIC_VFTABLE;
	return self;
}

cDesktopLocalLogic *__thiscall cDesktopLocalLogic_Delete(cDesktopLocalLogic *self, byte flags) {
	self->base.vftable = CDESKTOPLOCALLOGIC_VFTABLE;
	cGenCharLocalLogic_dtor(self);
	if (flags & 1) {
		operator_delete_sized(self, sizeof(*self));
	}
	return self;
}
