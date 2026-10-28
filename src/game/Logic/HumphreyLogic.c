/*
 * Logic/HumphreyLogic.cpp of the game: Humphrey, a functionary (cFunctionaryLocalLogic, include/scrap/logic.h) at
 * the desktop "HumphreyDesktop". The character "Humphrey_Default" takes its desktop at the first update.
 */
#include <string.h>

#include "scrap/chartype.h"

#define Humphrey_Vtbl(self) ((self)->base.base.vftable)

void *__thiscall cHumphreyLocalLogic_ctor(void *mem, cEntity *parent) {
	cFunctionaryLocalLogic *self = mem;

	cFunctionaryLocalLogic_ctor(self, parent);
	Humphrey_Vtbl(self) = CHUMPHREYLOCALLOGIC_VFTABLE;
	self->isDefault = !strcmp(parent->name, "Humphrey_Default");
	return self;
}

cFunctionaryLocalLogic *__thiscall cHumphreyLocalLogic_Delete(cFunctionaryLocalLogic *self, byte flags) {
	Humphrey_Vtbl(self) = CHUMPHREYLOCALLOGIC_VFTABLE;
	cFunctionaryLocalLogic_dtor(self);
	if (flags & 1) {
		operator_delete_sized(self, 0xbc);
	}
	return self;
}

/* vf40 */
void __thiscall cHumphreyLocalLogic_AssignDesktop(cFunctionaryLocalLogic *self, float maxDist) {
	(void)maxDist;
	if (cFunctionaryLocalLogic_GetDesktop(self)) {
		return;
	}
	cEntity *d = World_FindEntity("HumphreyDesktop");
	if (d) {
		cEntity_AddLink(d, &self->desktop);
	}
}

/* vf41 */
void __thiscall cHumphreyLocalLogic_ReleaseDesktop(cFunctionaryLocalLogic *self) {
	if (!cFunctionaryLocalLogic_GetDesktop(self)) {
		return;
	}
	cFunctionaryLocalLogic_FreeDesktop(self);
	CHECK(self->desktop.data);
	Nto1Node_Unlink(&self->desktop);
}

/* vf17: the default Humphrey takes its desktop once */
void __thiscall cHumphreyLocalLogic_Update(cFunctionaryLocalLogic *self, float dt) {
	if (self->isDefault && !cFunctionaryLocalLogic_GetDesktop(self)) {
		Humphrey_Vtbl(self)->AssignDesktop(self, 0.0f);
		self->isDefault = 0;
	}
	cFunctionaryLocalLogic_Update(self, dt);
}

/* vf38: the angles of the desktop */
vec3_t *__thiscall cHumphreyLocalLogic_GetWorkAng(cFunctionaryLocalLogic *self, vec3_t *out) {
	CHECK(cFunctionaryLocalLogic_GetDesktop(self));
	cEntity *d = cFunctionaryLocalLogic_GetDesktop(self);
	Obj_Vtbl(d)->GetAng(d, out);
	return out;
}
