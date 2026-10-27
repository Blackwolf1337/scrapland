/*
 * Entities/Char/Type/Sebastian.cpp of the game: Sebastian, whose flash rises from him (include/scrap/chartype.h).
 */
#include "scrap/chartype.h"

/* params: Sebastian, an event, a float: the flash effect for 2 seconds */
void ChType_FXSebastianFlash(cAbstractFile *params) {
	char name[0x80], event[0x80];

	ChType_ReadParams(params, name, event);
	cEntity *v = World_FindEntity(name);
	CHECK(v);
	CHECK(Entity_IsType(v, TYEN_WITH_LIFE));
	cObjElementFXSebastianFlash *el = FX_DropNew(g_FXSebastianFlashFile, 2.0f);
	if (el) {
		if (!el->started) {
			el->started = 1;
		}
		FXMisc_SetEntity(el, v);
		cObjElementFXSebastianFlash_RandomPoints(el);
	}
}
