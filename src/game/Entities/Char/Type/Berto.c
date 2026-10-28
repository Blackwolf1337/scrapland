/*
 * Entities/Char/Type/Berto.cpp of the game: Berto, whose crystal flashes the characters around him
 * (include/scrap/chartype.h). BertoActionPress is ChType_ActionPress, BertoAttack ChType_Attack.
 */
#include "scrap/chartype.h"

/* params: Berto, an event, a float */
void ChType_BertoActionEnd(cAbstractFile *params) {
	char name[0x80], event[0x80];

	ChType_ReadParams(params, name, event);
	cEntity *v = ChType_CheckChar(name);
	ChType_ActionSound(v, 0, 1);
}

void ChType_BertoActionInit(cAbstractFile *params) {
	char name[0x80], event[0x80];

	ChType_ReadParams(params, name, event);
	cEntity *v = ChType_CheckChar(name);
	ChType_ActionSound(v, 1, 0);
}

void ChType_BertoAttack(cAbstractFile *params) {
	ChType_Attack(params, g_BertoAmmoCost);
}

/* the flash test of the hit (OnFlash): params a flashed character, other than Berto, with the flash effect from
 * Berto to it and the flash sound at it */
void ChType_BertoFlashHit(cAbstractFile *params) {
	char name[0x80];
	vec3_t tmp;

	cAbstractFile_ReadString(params, name, 0x80);
	CHECK(g_BertoEnt);
	CHECK(Entity_IsType(g_BertoEnt, TYEN_WALKCHAR));
	if (!name[0]) {
		return;
	}
	cEntity *e = World_FindEntity(name);
	if (!e || g_BertoEnt == e || !e->vftable->IsType(e, TYEN_WALKCHAR) || WChar_God(e)) {
		return;
	}
	cObjElementFXBertoFlash *el = FX_DropNew(g_FXBertoFlashFile, 0.8f);
	if (el) {
		if (!el->started) {
			el->started = 1;
		}
		FXMisc_SetEntity(el, g_BertoEnt);
		FXCharActions_SetEntity8(el, e);
		cObjElementFXBertoFlash_RandomPoints(el);
	}
	const char *sound = g_BertoFlashSound;
	CSoundSys *snd = g_pSound;
	int vol = Snd_Volume(g_BertoFlashSoundVol);
	const vec3_t *pos = e->vftable->GetPos(e, &tmp);
	snd->vftable->PlayAt(g_pSound, sound, pos, vol);
}

/* params: Berto, an event, a float. The flash around him, ChType_BertoFlashHit for each flashed character. */
void ChType_BertoHitEvent(cAbstractFile *params) {
	char name[0x80], event[0x80];
	vec3_t tmp;

	ChType_ReadParams(params, name, event);
	g_BertoEnt = World_FindEntity(name);
	CHECK(g_BertoEnt);
	CHECK(Entity_IsType(g_BertoEnt, TYEN_WALKCHAR));
	if (WChar_Cam(g_BertoEnt)) {
		cPad_Rumble(&g_Pads[0], 0.0f, 0.5f, 0.25f);
	}
	cPy_Function_SetNative(g_OnFlash, (nativefunc_t)GAME_FUNC(ChType_BertoFlashHit, CHTYPE_BERTOFLASHHIT));
	float radius = g_BertoFlashDist;
	const vec3_t *pos = g_BertoEnt->vftable->GetPos(g_BertoEnt, &tmp);
	Logic_Flash(pos, radius, 1);
	cPy_Function_Clear(g_OnFlash);
}

void ChType_FXBertoAttack(cAbstractFile *params) {
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
