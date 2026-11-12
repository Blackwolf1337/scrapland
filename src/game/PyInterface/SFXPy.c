/*
 * Python module SFX (method table SFX_methods): effects. Original source: PyInterface/SFXPy.cpp.
 * Entities are given by name; a missing entity or one of the wrong type fails a CHECK like in the original.
 */
#include "scrap/fx.h"
#include "scrap/global.h"
#include "scrap/python.h"
#include "scrap/scriptapi.h"

/* ShipExplosion(V3D Pos, V3D Vel, float scale, float time, float nflames, bool bcolision) : Explosión de la nave */
API_FN(SFX, ShipExplosion) {
	vec3_t pos, vel;
	float scale = 1.0f, time = 1.0f;
	int numFlames = 0, collision = 0;
	if (!Mode_Parse(a, n, "(fff)(fff)ffii", &pos.x, &pos.y, &pos.z, &vel.x, &vel.y, &vel.z, &scale, &time, &numFlames,
			&collision)) {
		return 0;
	}
	FX_ShipExplosion(pos, vel, scale, time, numFlames, collision);
	return 1;
}

/* FXCharacterConversor(CharacterName, ConversorName, phase) : Conversor de Personajes */
API_FN(SFX, CharacterConversor) {
	const char *characterName, *conversorName;
	int phase = 1;
	if (!Mode_Parse(a, n, "ssi", &characterName, &conversorName, &phase)) {
		return 0;
	}
	cEntity *vCharacter = World_FindEntity(characterName);
	CHECK(vCharacter);
	CHECK(Entity_IsType(vCharacter, TYEN_WALKCHAR));
	cEntity *vConversor = World_FindEntity(conversorName);
	CHECK(vConversor);
	CHECK(Entity_IsType(vConversor, TYEN_WITH_LIFE));
	FX_CharacterConversor(vCharacter, vConversor, phase, 1.7f, 0.2f);
	return 1;
}

/* FXCharacterConversion(AttackerName, AttackedName, time) : Conversión de Personajes */
API_FN(SFX, CharacterConversion) {
	const char *attackerName, *attackedName;
	float time = 1.0f;
	if (!Mode_Parse(a, n, "ssf", &attackerName, &attackedName, &time)) {
		return 0;
	}
	cEntity *vAttacker = World_FindEntity(attackerName);
	CHECK(vAttacker);
	CHECK(Entity_IsType(vAttacker, TYEN_WITH_LIFE));
	cEntity *vAttacked = World_FindEntity(attackedName);
	CHECK(vAttacked);
	CHECK(Entity_IsType(vAttacked, TYEN_WITH_LIFE));
	FX_CharacterConversion(vAttacker, vAttacked, time);
	return 1;
}

/* FXBishopSellLife(AttackerName, AttackedName) : Efecto de dar vida (de Attacker [Bishop ó NULL] a Attacked) */
API_FN(SFX, BishopSellLife) {
	const char *attackerName, *attackedName;
	if (!Mode_Parse(a, n, "ss", &attackerName, &attackedName)) {
		return 0;
	}
	cEntity *vAttacker = World_FindEntity(attackerName);
	if (vAttacker) {
		CHECK(Entity_IsType(vAttacker, TYEN_WITH_LIFE));
	}
	cEntity *vAttacked = World_FindEntity(attackedName);
	CHECK(vAttacked);
	CHECK(Entity_IsType(vAttacked, TYEN_WITH_LIFE));
	FX_BishopSellLife(vAttacker, vAttacked);
	return 1;
}

/* FXMoneyTransfer(EntityFrom, EntityTo, Time) : Transferencia de dinero */
API_FN(SFX, MoneyTransfer) {
	const char *fromName, *toName;
	float time;
	if (!Mode_Parse(a, n, "ssf", &fromName, &toName, &time)) {
		return 0;
	}
	cEntity *vFrom = World_FindEntity(fromName);
	CHECK(vFrom);
	CHECK(Entity_IsType(vFrom, TYEN_WITH_LIFE));
	cEntity *vTo = World_FindEntity(toName);
	CHECK(vTo);
	CHECK(Entity_IsType(vTo, TYEN_WITH_LIFE));
	FX_MoneyTransfer(vFrom, vTo, time);
	return 1;
}

/* FXEmbeddedSet(EntityName, FXType) : Asigna el controlador de effects embedidos */
API_FN(SFX, EmbeddedSet) {
	const char *entityName, *fxType;
	if (!Mode_Parse(a, n, "ss", &entityName, &fxType)) {
		return 0;
	}
	cEntity *vEntity = World_FindEntity(entityName);
	CHECK(vEntity);
	Entity_SetEmbeddedFX(vEntity, fxType);
	return 1;
}

static char str_PyDoc_SFX_BishopSellLife[];
static char str_PyDoc_SFX_CharacterConversion[88];
static char str_PyDoc_SFX_CharacterConversor[];
static char str_PyDoc_SFX_EmbeddedSet[];
static char str_PyDoc_SFX_MoneyTransfer[];
static char str_PyDoc_SFX_ShipExplosion[];
static const portFn_t SFX_methods[] = {
	PORT_FND(ShipExplosion, SFX_ShipExplosion, str_PyDoc_SFX_ShipExplosion),
	PORT_FND(CharacterConversor, SFX_CharacterConversor, str_PyDoc_SFX_CharacterConversor),
	PORT_FND(CharacterConversion, SFX_CharacterConversion, str_PyDoc_SFX_CharacterConversion),
	PORT_FND(BishopSellLife, SFX_BishopSellLife, str_PyDoc_SFX_BishopSellLife),
	PORT_FND(MoneyTransfer, SFX_MoneyTransfer, str_PyDoc_SFX_MoneyTransfer),
	PORT_FND(EmbeddedSet, SFX_EmbeddedSet, str_PyDoc_SFX_EmbeddedSet),
	PORT_FNEND
};

SD_PY_MODULE(SFX, 0x42d060)


static char str_PyDoc_SFX_ShipExplosion[] =
	"ShipExplosion(V3D Pos, V3D Vel, float scale, float time, float nflames, bool bcolision) : Explosión "
	"de la nave.";

static char str_PyDoc_SFX_CharacterConversor[] =
	"FXCharacterConversor(CharacterName, ConversorName, phase) : Conversor de Personajes.";

static char str_PyDoc_SFX_CharacterConversion[88] =
	"FXCharacterConversion(AttackerName, AttackedName, time) : Conversión de Personajes.";

static char str_PyDoc_SFX_BishopSellLife[] =
	"FXBishopSellLife(AttackerName, AttackedName) : Efecto de dar vida (de Attacker [Bishop ó NULL] a Att"
	"acked[Usuario]).";

static char str_PyDoc_SFX_MoneyTransfer[] =
	"FXMoneyTransfer(EntityFrom, EntityTo, Time) : Transferencia de dinero";

static char str_PyDoc_SFX_EmbeddedSet[] =
	"FXEmbeddedSet(EntityName, FXType) : Asigna el controlador de efectos embedidos.";

PORTS(PyInterface_SFXPy)
