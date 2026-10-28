/*
 * Logic/BettyLogic.cpp of the game: Betty, a functionary at the desktop "BettyDesktop" (include/scrap/logic.h); the
 * release, the update and the slots without code are the ones of Berto (BertoLogic.c).
 */
#include <string.h>

#include "scrap/logic.h"
#include "scrap/objentity.h"

cFunctionaryLocalLogic *__thiscall cBettyLocalLogic_ctor(cFunctionaryLocalLogic *self, cEntity *parent) {
	cFunctionaryLocalLogic_ctor(self, parent);
	self->base.base.vftable = CBETTYLOCALLOGIC_VFTABLE;
	self->isDefault = !strcmp(parent->name, "Betty_Default");
	return self;
}

cFunctionaryLocalLogic *__thiscall cBettyLocalLogic_Delete(cFunctionaryLocalLogic *self, byte flags) {
	self->base.base.vftable = CBETTYLOCALLOGIC_VFTABLE;
	cFunctionaryLocalLogic_dtor(self);
	if (flags & 1) {
		operator_delete_sized(self, sizeof(*self));
	}
	return self;
}

/* vf40 */
void __thiscall cBettyLocalLogic_AssignDesktop(cFunctionaryLocalLogic *self, int arg) {
	(void)arg;
	Logic_LinkDesktop(self, "BettyDesktop");
}

/* vf38 (the callers use the returned out): the angles at the desktop, turned by a quarter the other way */
vec3_t *__thiscall cBettyLocalLogic_GetWorkAng(cFunctionaryLocalLogic *self, vec3_t *out) {
	vec3_t tmp;

	CHECK(cFunctionaryLocalLogic_GetDesktop(self));
	cEntity *d = cFunctionaryLocalLogic_GetDesktop(self);
	const vec3_t *a = Obj_Vtbl(d)->GetAng(d, &tmp);
	out->x = a->x + 0.0f;
	out->y = a->y + 1.57079637f;
	out->z = a->z + 0.0f;
	return out;
}
