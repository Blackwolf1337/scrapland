/*
 * Entities/Char/Type/Sebastian.cpp of the game: Sebastian, whose flash rises from him (include/scrap/chartype.h).
 */
#include "scrap/chartype.h"
#include "scrap/staticinit.h"

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

cGVar GVar_FXSebastianFlashFile;
cPy_NativeFunc NativeFunc_FXSebastianFlash;

/* the dynamic initializers of the globals here, run in the order of src/standalone/crtxc.inc */
FINI_GVAR_STRING(Fini_GVar_FXSebastianFlashFile, &GVar_FXSebastianFlashFile)
INIT_GVAR_STRING(Init_GVar_FXSebastianFlashFile, Fini_GVar_FXSebastianFlashFile, &GVar_FXSebastianFlashFile, "FXSebastianFlashFile", 0, "Default Model for Sebastian Flash Effect", "Models/GFX/Sebastian/Flash.M3D")
INIT_NATIVE_FUNC(Init_NativeFunc_FXSebastianFlash, &NativeFunc_FXSebastianFlash, "FXSebastianFlash", ChType_FXSebastianFlash)
