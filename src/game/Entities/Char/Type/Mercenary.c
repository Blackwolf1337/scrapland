/*
 * Entities/Char/Type/Mercenary.cpp of the game: the mercenary, who drinks mercury from his jar
 * (include/scrap/chartype.h).
 */
#include "scrap/chartype.h"

/* params: the mercenary, an event, a float: the drink effect for 2 seconds */
void ChType_FXMercenaryDrink(cAbstractFile *params) {
	char name[0x80], event[0x80];
	vec3_t tmp;

	ChType_ReadParams(params, name, event);
	cEntity *v = World_FindEntity(name);
	CHECK(v);
	CHECK(Entity_IsType(v, TYEN_WITH_LIFE));
	cDebrisSys *sys = cWorld_GetDebrisSys(g_pWorld, g_FXMercenaryDrinkFile);
	cDebris *d = cDebrisSys_NewElement(sys);
	CGameModel_SetPos(Debris_Model(d), v->vftable->GetPos(v, &tmp));
	Debris_Life(d) = 2.0f;
	cDebrisSys_Drop(sys, d);
	cObjElementFXMercenaryDrink *el = Debris_Element(d);
	if (el) {
		if (!el->started) {
			el->started = 1;
		}
		cObjElementFXMercenaryDrink_Start(el, v);
		cObjElementFXMercenaryDrink_SetC0(el, g_MercenaryDrinkPosX, g_MercenaryDrinkPosY, g_MercenaryDrinkPosZ);
	}
}
