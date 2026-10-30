/*
 * Logic/BertoLogic.cpp of the game: Berto, a functionary (cFunctionaryLocalLogic) at the desktop "BertoDesktop"
 * (include/scrap/logic.h). Berto_Default takes the desktop on its first update. BettyLogic.c is the same for Betty.
 */
#include <string.h>

#include "scrap/logic.h"
#include "scrap/objentity.h"

cBertoLocalLogic *__thiscall cBertoLocalLogic_ctor(cBertoLocalLogic *self, cEntity *parent) {
	cFunctionaryLocalLogic_ctor(&self->base, parent);
	self->base.base.base.vftable = CBERTOLOCALLOGIC_VFTABLE;
	self->isDefault = !strcmp(parent->name, "Berto_Default");
	return self;
}

cBertoLocalLogic *__thiscall cBertoLocalLogic_Delete(cBertoLocalLogic *self, byte flags) {
	self->base.base.base.vftable = CBERTOLOCALLOGIC_VFTABLE;
	cFunctionaryLocalLogic_dtor(&self->base);
	if (flags & 1) {
		operator_delete_sized(self, sizeof(*self));
	}
	return self;
}

/* the desktop of the name linked, when the functionary has none (also of Betty) */
void Logic_LinkDesktop(cFunctionaryLocalLogic *self, const char *name) {
	if (!cFunctionaryLocalLogic_GetDesktop(self)) {
		cEntity *d = World_FindEntity(name);
		if (d) {
			cEntity_AddLink(d, &self->desktop);
		}
	}
}

/* vf40 */
void __thiscall cBertoLocalLogic_AssignDesktop(cBertoLocalLogic *self, int arg) {
	(void)arg;
	Logic_LinkDesktop(&self->base, "BertoDesktop");
}

/* vf41 (also of Betty) */
void __thiscall cBertoLocalLogic_ReleaseDesktop(cBertoLocalLogic *self) {
	if (cFunctionaryLocalLogic_GetDesktop(&self->base)) {
		cFunctionaryLocalLogic_FreeDesktop(&self->base);
		CHECK(self->base.desktop.data);
		Nto1Node_Unlink(&self->base.desktop);
	}
}

/* vf17 (also of Betty): the default character takes its desktop */
void __thiscall cBertoLocalLogic_Update(cBertoLocalLogic *self, float dt) {
	if (self->isDefault && !cFunctionaryLocalLogic_GetDesktop(&self->base)) {
		self->base.base.base.vftable->AssignDesktop(self, 0);
		self->isDefault = 0;
	}
	cFunctionaryLocalLogic_Update(&self->base, dt);
}

/* vf38 (the callers use the returned out): the angles at the desktop, turned by a quarter */
vec3_t *__thiscall cBertoLocalLogic_GetWorkAng(cBertoLocalLogic *self, vec3_t *out) {
	vec3_t tmp;

	CHECK(cFunctionaryLocalLogic_GetDesktop(&self->base));
	cEntity *d = cFunctionaryLocalLogic_GetDesktop(&self->base);
	const vec3_t *a = Obj_Vtbl(d)->GetAng(d, &tmp);
	out->x = a->x - 0.0f;
	out->y = a->y - 1.57079637f;
	out->z = a->z - 0.0f;
	return out;
}
