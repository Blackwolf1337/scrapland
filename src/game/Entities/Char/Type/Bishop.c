/*
 * Entities/Char/Type/Bishop.cpp of the game: the bishop, who sells life (include/scrap/chartype.h).
 * BishopActionPress is ChType_ActionPress.
 */
#include "scrap/chartype.h"
#include "scrap/fx.h"

/* params: the bishop, an event, a float: the action again while the button is down and the energy lasts (always
 * for an AI) */
void ChType_EndActionEnd(cAbstractFile *params) {
	char name[0x80], event[0x80];

	ChType_ReadParams(params, name, event);
	cEntity *v = World_FindEntity(name);
	cEntity *usr = ChType_Usr0();
	CHECK(v);
	CHECK(Entity_IsType(v, TYEN_WALKCHAR));
	if ((usr != v || !(WChar_EnergyBarDecr(v) > 1.0f - WChar_EnergyUsed(v))) && WChar_ActionPress(v)) {
		cActCtrl_Play(ChType_ActCtrl(v), "ActionInit", 1, 1.0f);
		cActCtrl_SetDefLoop(ChType_ActCtrl(v), "Action", 1);
	}
}

/* params: the attacker (none: the life ring only) and the attacked */
void ChType_FXBishopSellLife(cAbstractFile *params) {
	char a[0x80], b[0x80];

	cAbstractFile_ReadString(params, a, 0x80);
	cAbstractFile_ReadString(params, b, 0x80);
	cEntity *attacker = World_FindEntity(a);
	cEntity *attacked = World_FindEntity(b);
	FX_BishopSellLife(attacker, attacked);
}

void FX_BishopSellLife(cEntity *attacker, cEntity *attacked) {
	if (attacker) {
		CHECK(Entity_IsType(attacker, TYEN_WITH_LIFE));
	}
	CHECK(attacked);
	CHECK(Entity_IsType(attacked, TYEN_WITH_LIFE));
	cObjElementFXBishopSellLife *el = FX_DropNew(g_FXBishopSellLifeFile, attacker ? 6.0f : 2.0f);
	if (el) {
		if (!el->started) {
			el->started = 1;
		}
		cObjElementFXBishopSellLife_SetAttacker(el, attacker);
		cObjElementFXBishopSellLife_SetAttacked(el, attacked);
	}
}
