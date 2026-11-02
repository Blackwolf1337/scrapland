/*
 * Entities/Char/Type/Mercenary.cpp of the game: the mercenary, who drinks mercury from his jar
 * (include/scrap/chartype.h).
 */
#include "scrap/chartype.h"
#include "scrap/staticinit.h"

#define g_FXMercenaryDrinkFile (GVar_FXMercenaryDrinkFile.value.s)
#define g_MercenaryDrinkPosX (GVar_MercenaryDrinkPosX.value.f)
#define g_MercenaryDrinkPosY (GVar_MercenaryDrinkPosY.value.f)
#define g_MercenaryDrinkPosZ (GVar_MercenaryDrinkPosZ.value.f)
static cGVar GVar_MercenaryDrinkPosZ;
static cGVar GVar_MercenaryDrinkPosY;
static cGVar GVar_MercenaryDrinkPosX;
static cGVar GVar_FXMercenaryDrinkFile;

/* params: the mercenary, an event, a float: the drink effect for 2 seconds */
static void ChType_FXMercenaryDrink(cAbstractFile *params) {
	char name[0x80], event[0x80];
	vec3_t tmp;

	ChType_ReadParams(params, name, event);
	cEntity *v = World_FindEntity(name);
	CHECK(v);
	CHECK(Entity_IsType(v, TYEN_WITH_LIFE));
	cDebrisSys *sys = cWorld_GetDebrisSys(g_pWorld, g_FXMercenaryDrinkFile);
	cDebris *d = cDebrisSys_NewElement(sys);
	CGameModel_SetPos(Debris_Model(d), v->vftable->GetPos(v, &tmp));
	d->life = 2.0f;
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

static cGVar GVar_MercenaryDrinkPosY;
static cGVar GVar_FXMercenaryDrinkFile;
static cGVar GVar_MercenaryDrinkPosX;
static cGVar GVar_MercenaryDrinkPosZ;
static cPy_NativeFunc NativeFunc_FXMercenaryDrink;

/* the dynamic initializers of the globals here, run in the order of src/standalone/crtxc.inc */
INIT_GVAR_FLOAT(Init_GVar_MercenaryDrinkPosX, &GVar_MercenaryDrinkPosX, "MercenaryDrinkPosX", 0, "Mercenary Drink Displacement X from center", 14.0f)
INIT_GVAR_FLOAT(Init_GVar_MercenaryDrinkPosY, &GVar_MercenaryDrinkPosY, "MercenaryDrinkPosY", 0, "Mercenary Drink Displacement Y from center", 2.0f)
INIT_GVAR_FLOAT(Init_GVar_MercenaryDrinkPosZ, &GVar_MercenaryDrinkPosZ, "MercenaryDrinkPosZ", 0, "Mercenary Drink Displacement Z from center", 28.0f)
FINI_GVAR_STRING(Fini_GVar_FXMercenaryDrinkFile, &GVar_FXMercenaryDrinkFile)
INIT_GVAR_STRING(Init_GVar_FXMercenaryDrinkFile, Fini_GVar_FXMercenaryDrinkFile, &GVar_FXMercenaryDrinkFile, "FXMercenaryDrinkFile", 0, "Default Model for Mercenary Drink Effects", "Models/GFX/Mercenary/Drink.M3D")
INIT_NATIVE_FUNC(Init_NativeFunc_FXMercenaryDrink, &NativeFunc_FXMercenaryDrink, "FXMercenaryDrink", ChType_FXMercenaryDrink)
