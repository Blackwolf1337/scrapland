/*
 * Logic/HumphreyLogic.cpp of the game: Humphrey, a functionary (cFunctionaryLocalLogic, include/scrap/logic.h) at
 * the desktop "HumphreyDesktop". The character "Humphrey_Default" takes its desktop at the first update.
 */
#include <string.h>

#include "scrap/chartype.h"

#define Humphrey_Vtbl(self) ((self)->base.base.base.vftable)

cHumphreyLocalLogic *__thiscall cHumphreyLocalLogic_ctor(cHumphreyLocalLogic *self, cEntity *parent) {
	cFunctionaryLocalLogic_ctor(&self->base, parent);
	Humphrey_Vtbl(self) = CHUMPHREYLOCALLOGIC_VFTABLE;
	self->isDefault = !strcmp(parent->name, "Humphrey_Default");
	return self;
}

cHumphreyLocalLogic *__thiscall cHumphreyLocalLogic_Delete(cHumphreyLocalLogic *self, byte flags) {
	Humphrey_Vtbl(self) = CHUMPHREYLOCALLOGIC_VFTABLE;
	cFunctionaryLocalLogic_dtor(&self->base);
	if (flags & 1) {
		operator_delete_sized(self, sizeof(*self));
	}
	return self;
}

/* vf40 */
void __thiscall cHumphreyLocalLogic_AssignDesktop(cHumphreyLocalLogic *self, float maxDist) {
	(void)maxDist;
	if (cFunctionaryLocalLogic_GetDesktop(&self->base)) {
		return;
	}
	cEntity *d = World_FindEntity("HumphreyDesktop");
	if (d) {
		cEntity_AddLink(d, &self->base.desktop);
	}
}

/* vf41 */
void __thiscall cHumphreyLocalLogic_ReleaseDesktop(cHumphreyLocalLogic *self) {
	if (!cFunctionaryLocalLogic_GetDesktop(&self->base)) {
		return;
	}
	cFunctionaryLocalLogic_FreeDesktop(&self->base);
	CHECK(self->base.desktop.data);
	Nto1Node_Unlink(&self->base.desktop);
}

/* vf17: the default Humphrey takes its desktop once */
void __thiscall cHumphreyLocalLogic_Update(cHumphreyLocalLogic *self, float dt) {
	if (self->isDefault && !cFunctionaryLocalLogic_GetDesktop(&self->base)) {
		Humphrey_Vtbl(self)->AssignDesktop(self, 0.0f);
		self->isDefault = 0;
	}
	cFunctionaryLocalLogic_Update(&self->base, dt);
}

/* vf38: the angles of the desktop */
vec3_t *__thiscall cHumphreyLocalLogic_GetWorkAng(cHumphreyLocalLogic *self, vec3_t *out) {
	CHECK(cFunctionaryLocalLogic_GetDesktop(&self->base));
	cEntity *d = cFunctionaryLocalLogic_GetDesktop(&self->base);
	Obj_Vtbl(d)->GetAng(d, out);
	return out;
}
