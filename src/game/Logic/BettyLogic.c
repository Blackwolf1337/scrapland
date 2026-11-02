/*
 * Logic/BettyLogic.cpp of the game: Betty, a functionary at the desktop "BettyDesktop" (include/scrap/logic.h); the
 * release, the update and the slots without code are the ones of Berto (BertoLogic.c).
 */
#include <string.h>

#include "scrap/logic.h"
#include "scrap/objentity.h"

cBettyLocalLogic *cBettyLocalLogic_ctor(cBettyLocalLogic *self, cEntity *parent) {
	cFunctionaryLocalLogic_ctor(&self->base, parent);
	self->base.base.base.vftable = CBETTYLOCALLOGIC_VFTABLE;
	self->isDefault = !strcmp(parent->name, "Betty_Default");
	return self;
}

cBettyLocalLogic *cBettyLocalLogic_Delete(cBettyLocalLogic *self, byte flags) {
	self->base.base.base.vftable = CBETTYLOCALLOGIC_VFTABLE;
	cFunctionaryLocalLogic_dtor(&self->base);
	if (flags & 1) {
		operator_delete_sized(self, sizeof(*self));
	}
	return self;
}

/* vf40 */
void cBettyLocalLogic_AssignDesktop(cBettyLocalLogic *self, float maxDist) {
	(void)maxDist;
	Logic_LinkDesktop(&self->base, "BettyDesktop");
}

/* vf38 (the callers use the returned out): the angles at the desktop, turned by a quarter the other way */
vec3_t *cBettyLocalLogic_GetWorkAng(cBettyLocalLogic *self, vec3_t *out) {
	vec3_t tmp;

	CHECK(cFunctionaryLocalLogic_GetDesktop(&self->base));
	cEntity *d = cFunctionaryLocalLogic_GetDesktop(&self->base);
	const vec3_t *a = Obj_Vtbl(d)->GetAng(d, &tmp);
	out->x = a->x + 0.0f;
	out->y = a->y + 1.57079637f;
	out->z = a->z + 0.0f;
	return out;
}
