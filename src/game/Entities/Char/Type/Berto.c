/*
 * Entities/Char/Type/Berto.cpp of the game: Berto, whose crystal flashes the characters around him
 * (include/scrap/chartype.h). BertoActionPress is ChType_ActionPress, BertoAttack ChType_Attack.
 */
#include "scrap/chartype.h"
#include "scrap/fx.h"
#include "scrap/staticinit.h"

#define g_FXBertoFlashFile (GVar_FXBertoFlashFile.value.s)
#define g_BertoFlashSound (GVar_BertoFlashSound.value.s)
#define g_BertoFlashDist (GVar_BertoFlashDist.value.f)
#define g_BertoFlashSoundVol (GVar_BertoFlashSoundVol.value.f)
#define g_BertoAmmoCost (GVar_BertoAmmoCost.value.f)
#define g_FXBertoAttackFile (GVar_FXBertoAttackFile.value.s)
static cGVar GVar_FXBertoFlashFile;
static cGVar GVar_FXBertoAttackFile;
static cGVar GVar_BertoFlashSoundVol;
static cGVar GVar_BertoFlashSound;
static cGVar GVar_BertoFlashDist;
static cGVar GVar_BertoAmmoCost;

/* params: Berto, an event, a float */
static void ChType_BertoActionEnd(cAbstractFile *params) {
	char name[0x80], event[0x80];

	ChType_ReadParams(params, name, event);
	cEntity *v = ChType_CheckChar(name);
	ChType_ActionSound(v, 0, 1);
}

static void ChType_BertoActionInit(cAbstractFile *params) {
	char name[0x80], event[0x80];

	ChType_ReadParams(params, name, event);
	cEntity *v = ChType_CheckChar(name);
	ChType_ActionSound(v, 1, 0);
}

static void ChType_BertoAttack(cAbstractFile *params) {
	ChType_Attack(params, g_BertoAmmoCost);
}

static cEntity *g_BertoEnt;

/* the flash test of the hit (OnFlash): params a flashed character, other than Berto, with the flash effect from
 * Berto to it and the flash sound at it */
static void ChType_BertoFlashHit(cAbstractFile *params) {
	char name[0x80];

	cAbstractFile_ReadString(params, name, 0x80);
	CHECK(g_BertoEnt);
	CHECK(Entity_IsType(g_BertoEnt, TYEN_WALKCHAR));
	if (!name[0]) {
		return;
	}
	cEntity *e = World_FindEntity(name);
	if (!e || g_BertoEnt == e || !Entity_IsType(e, TYEN_WALKCHAR) || WChar(e)->wl.god) {
		return;
	}
	FX_BertoFlash(g_BertoEnt, e);
}

void FX_BertoFlash(cEntity *from, cEntity *e) {
	vec3_t tmp;
	cObjElementFXBertoFlash *el = FX_DropNew(g_FXBertoFlashFile, 0.8f);
	if (el) {
		if (!el->started) {
			el->started = 1;
		}
		FXMisc_SetEntity(el, from);
		FXCharActions_SetEntity8(el, e);
		cObjElementFXBertoFlash_RandomPoints(el);
	}
	const char *sound = g_BertoFlashSound;
	int vol = Snd_Volume(g_BertoFlashSoundVol);
	const vec3_t *pos = Entity_GetPos(e, &tmp);
	SoundSys_PlayAt(g_pSound, sound, pos, vol);
}

/* params: Berto, an event, a float. The flash around him, ChType_BertoFlashHit for each flashed character. */
static void ChType_BertoHitEvent(cAbstractFile *params) {
	char name[0x80], event[0x80];
	vec3_t tmp;

	ChType_ReadParams(params, name, event);
	g_BertoEnt = World_FindEntity(name);
	CHECK(g_BertoEnt);
	CHECK(Entity_IsType(g_BertoEnt, TYEN_WALKCHAR));
	if (WChar(g_BertoEnt)->wl.obj.cam) {
		cPad_Rumble(&g_Pads[0], 0.0f, 0.5f, 0.25f);
	}
	cPy_Function_SetNative(g_OnFlash, (nativefunc_t)ChType_BertoFlashHit);
	float radius = g_BertoFlashDist;
	const vec3_t *pos = Entity_GetPos(g_BertoEnt, &tmp);
	Logic_Flash(pos, radius, 1);
	cPy_Function_Clear(g_OnFlash);
}

static void ChType_FXBertoAttack(cAbstractFile *params) {
	char name[0x80], event[0x80];

	ChType_ReadParams(params, name, event);
	cEntity *v = ChType_CheckChar(name);
	cObjElementFXBertoAttack *el = FX_DropNew(g_FXBertoAttackFile, 1.0f);
	if (el) {
		if (!el->started) {
			el->started = 1;
		}
		cObjElementFXBertoAttack_Start(el, v);
	}
}

static cGVar GVar_FXBertoFlashFile;
static cGVar GVar_BertoFlashSound;
static cGVar GVar_BertoFlashDist;
static cGVar GVar_BertoFlashSoundVol;
static cGVar GVar_BertoAmmoCost;
static cGVar GVar_FXBertoAttackFile;
static cPy_NativeFunc NativeFunc_BertoAttack;
static cPy_NativeFunc NativeFunc_BertoHitEvent;
static cPy_NativeFunc NativeFunc_FXBertoAttack;
static cPy_NativeFunc NativeFunc_BertoActionInit;
static cPy_NativeFunc NativeFunc_BertoActionEnd;

/* the dynamic initializers of the globals here, run in the order of src/standalone/crtxc.inc */
INIT_GVAR_FLOAT(Init_GVar_BertoAmmoCost, &GVar_BertoAmmoCost, "BertoAmmoCost", 0, "Berto AmmoCost Per shoot", 0.03f)
INIT_GVAR_FLOAT(Init_GVar_BertoFlashDist, &GVar_BertoFlashDist, "BertoFlashDist", 0, "Berto Flash Max Distance", 400.0f)
FINI_GVAR_STRING(Fini_GVar_FXBertoAttackFile, &GVar_FXBertoAttackFile)
INIT_GVAR_STRING(Init_GVar_FXBertoAttackFile, Fini_GVar_FXBertoAttackFile, &GVar_FXBertoAttackFile, "FXBertoAttackFile", 0, "Default Model for Berto Attack Effect", "Models/GFX/Berto/Attack.M3D")
FINI_GVAR_STRING(Fini_GVar_FXBertoFlashFile, &GVar_FXBertoFlashFile)
INIT_GVAR_STRING(Init_GVar_FXBertoFlashFile, Fini_GVar_FXBertoFlashFile, &GVar_FXBertoFlashFile, "FXBertoFlashFile", 0, "Default Model for Berto Flash Effect", "Models/GFX/Berto/Flash.M3D")
FINI_GVAR_STRING(Fini_GVar_BertoFlashSound, &GVar_BertoFlashSound)
INIT_GVAR_STRING(Init_GVar_BertoFlashSound, Fini_GVar_BertoFlashSound, &GVar_BertoFlashSound, "BertoFlashSound", 0, "Berto Flash sound name", "BertoFlash")
INIT_GVAR_FLOAT(Init_GVar_BertoFlashSoundVol, &GVar_BertoFlashSoundVol, "BertoFlashSoundVol", 0, "Berto Flash sound volume", 1.0f)
INIT_NATIVE_FUNC(Init_NativeFunc_BertoAttack, &NativeFunc_BertoAttack, "BertoAttack", ChType_BertoAttack)
INIT_NATIVE_FUNC(Init_NativeFunc_BertoHitEvent, &NativeFunc_BertoHitEvent, "BertoHitEvent", ChType_BertoHitEvent)
INIT_NATIVE_FUNC(Init_NativeFunc_FXBertoAttack, &NativeFunc_FXBertoAttack, "FXBertoAttack", ChType_FXBertoAttack)
INIT_NATIVE_FUNC(Init_NativeFunc_BertoActionInit, &NativeFunc_BertoActionInit, "BertoActionInit", ChType_BertoActionInit)
INIT_NATIVE_FUNC(Init_NativeFunc_BertoActionEnd, &NativeFunc_BertoActionEnd, "BertoActionEnd", ChType_BertoActionEnd)
