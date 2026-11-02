/*
 * Python module SFX (method table SFX_methods, rva 0x42d060): effects. Original source: PyInterface/SFXPy.cpp.
 * Entities are given by name; a missing entity or one of the wrong type fails a CHECK like in the original.
 */
#include "scrap/fx.h"
#include "scrap/global.h"
#include "scrap/python.h"

/* ShipExplosion(V3D Pos, V3D Vel, float scale, float time, float nflames, bool bcolision) : Explosión de la nave */
PyObject *SFX_ShipExplosion(PyObject *self, PyObject *args) {
	vec3_t pos, vel;
	float scale = 1.0f, time = 1.0f;
	int numFlames = 0, collision = 0;

	(void)self;
	if (!PyArg_ParseTuple(args, "(fff)(fff)ffii", &pos.x, &pos.y, &pos.z, &vel.x, &vel.y, &vel.z, &scale, &time,
			&numFlames, &collision)) {
		return NULL;
	}
	FX_ShipExplosion(pos, vel, scale, time, numFlames, collision);
	return Py_ReturnNone();
}

/* FXCharacterConversor(CharacterName, ConversorName, phase) : Conversor de Personajes */
PyObject *SFX_CharacterConversor(PyObject *self, PyObject *args) {
	char *characterName, *conversorName;
	int phase = 1;

	(void)self;
	if (!PyArg_ParseTuple(args, "ssi", &characterName, &conversorName, &phase)) {
		return NULL;
	}
	cEntity *vCharacter = World_FindEntity(characterName);
	CHECK(vCharacter);
	CHECK(Entity_IsType(vCharacter, TYEN_WALKCHAR));
	cEntity *vConversor = World_FindEntity(conversorName);
	CHECK(vConversor);
	CHECK(Entity_IsType(vConversor, TYEN_WITH_LIFE));
	FX_CharacterConversor(vCharacter, vConversor, phase, 1.7f, 0.2f);
	return Py_ReturnNone();
}

/* FXCharacterConversion(AttackerName, AttackedName, time) : Conversión de Personajes */
PyObject *SFX_CharacterConversion(PyObject *self, PyObject *args) {
	char *attackerName, *attackedName;
	float time = 1.0f;

	(void)self;
	if (!PyArg_ParseTuple(args, "ssf", &attackerName, &attackedName, &time)) {
		return NULL;
	}
	cEntity *vAttacker = World_FindEntity(attackerName);
	CHECK(vAttacker);
	CHECK(Entity_IsType(vAttacker, TYEN_WITH_LIFE));
	cEntity *vAttacked = World_FindEntity(attackedName);
	CHECK(vAttacked);
	CHECK(Entity_IsType(vAttacked, TYEN_WITH_LIFE));
	FX_CharacterConversion(vAttacker, vAttacked, time);
	return Py_ReturnNone();
}

/* FXBishopSellLife(AttackerName, AttackedName) : Efecto de dar vida (de Attacker [Bishop ó NULL] a Attacked) */
PyObject *SFX_BishopSellLife(PyObject *self, PyObject *args) {
	char *attackerName, *attackedName;

	(void)self;
	if (!PyArg_ParseTuple(args, "ss", &attackerName, &attackedName)) {
		return NULL;
	}
	cEntity *vAttacker = World_FindEntity(attackerName);
	if (vAttacker) {
		CHECK(Entity_IsType(vAttacker, TYEN_WITH_LIFE));
	}
	cEntity *vAttacked = World_FindEntity(attackedName);
	CHECK(vAttacked);
	CHECK(Entity_IsType(vAttacked, TYEN_WITH_LIFE));
	FX_BishopSellLife(vAttacker, vAttacked);
	return Py_ReturnNone();
}

/* FXMoneyTransfer(EntityFrom, EntityTo, Time) : Transferencia de dinero */
PyObject *SFX_MoneyTransfer(PyObject *self, PyObject *args) {
	char *fromName, *toName;
	float time;

	(void)self;
	if (!PyArg_ParseTuple(args, "ssf", &fromName, &toName, &time)) {
		return NULL;
	}
	cEntity *vFrom = World_FindEntity(fromName);
	CHECK(vFrom);
	CHECK(Entity_IsType(vFrom, TYEN_WITH_LIFE));
	cEntity *vTo = World_FindEntity(toName);
	CHECK(vTo);
	CHECK(Entity_IsType(vTo, TYEN_WITH_LIFE));
	FX_MoneyTransfer(vFrom, vTo, time);
	return Py_ReturnNone();
}

/* FXEmbeddedSet(EntityName, FXType) : Asigna el controlador de effects embedidos */
PyObject *SFX_EmbeddedSet(PyObject *self, PyObject *args) {
	char *entityName, *fxType;

	(void)self;
	if (!PyArg_ParseTuple(args, "ss", &entityName, &fxType)) {
		return NULL;
	}
	cEntity *vEntity = World_FindEntity(entityName);
	CHECK(vEntity);
	vEntity->vftable->SetEmbeddedFX(vEntity, fxType);
	return Py_ReturnNone();
}

extern char str_PyDoc_SFX_BishopSellLife[];
extern char str_PyDoc_SFX_CharacterConversion[88];
extern char str_PyDoc_SFX_CharacterConversor[];
extern char str_PyDoc_SFX_EmbeddedSet[];
extern char str_PyDoc_SFX_MoneyTransfer[];
extern char str_PyDoc_SFX_ShipExplosion[];
extern const char str_PyMethod_SFX_BishopSellLife[];
extern const char str_PyMethod_SFX_CharacterConversion[];
extern const char str_PyMethod_SFX_CharacterConversor[];
extern const char str_PyMethod_SFX_EmbeddedSet[];
extern const char str_PyMethod_SFX_MoneyTransfer[];
extern const char str_PyMethod_SFX_ShipExplosion[];
PyMethodDef SFX_methods[7] = {
	{
		.ml_name = (void *)str_PyMethod_SFX_ShipExplosion,
		.ml_meth = (void *)SFX_ShipExplosion,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_SFX_ShipExplosion,
	},
	{
		.ml_name = (void *)str_PyMethod_SFX_CharacterConversor,
		.ml_meth = (void *)SFX_CharacterConversor,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_SFX_CharacterConversor,
	},
	{
		.ml_name = (void *)str_PyMethod_SFX_CharacterConversion,
		.ml_meth = (void *)SFX_CharacterConversion,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_SFX_CharacterConversion,
	},
	{
		.ml_name = (void *)str_PyMethod_SFX_BishopSellLife,
		.ml_meth = (void *)SFX_BishopSellLife,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_SFX_BishopSellLife,
	},
	{
		.ml_name = (void *)str_PyMethod_SFX_MoneyTransfer,
		.ml_meth = (void *)SFX_MoneyTransfer,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_SFX_MoneyTransfer,
	},
	{
		.ml_name = (void *)str_PyMethod_SFX_EmbeddedSet,
		.ml_meth = (void *)SFX_EmbeddedSet,
		.ml_flags = 1,
		.ml_doc = (void *)str_PyDoc_SFX_EmbeddedSet,
	},
};

SD_PY_MODULE(SFX, 0x42d060)

const char str_PyMethod_SFX_ShipExplosion[] = "ShipExplosion";
const char str_PyMethod_SFX_CharacterConversor[] = "CharacterConversor";
const char str_PyMethod_SFX_CharacterConversion[] = "CharacterConversion";
const char str_PyMethod_SFX_BishopSellLife[] = "BishopSellLife";
const char str_PyMethod_SFX_MoneyTransfer[] = "MoneyTransfer";
const char str_PyMethod_SFX_EmbeddedSet[] = "EmbeddedSet";

char str_PyDoc_SFX_ShipExplosion[] =
	"ShipExplosion(V3D Pos, V3D Vel, float scale, float time, float nflames, bool bcolision) : Explosión "
	"de la nave.";

char str_PyDoc_SFX_CharacterConversor[] =
	"FXCharacterConversor(CharacterName, ConversorName, phase) : Conversor de Personajes.";

char str_PyDoc_SFX_CharacterConversion[88] =
	"FXCharacterConversion(AttackerName, AttackedName, time) : Conversión de Personajes.";

char str_PyDoc_SFX_BishopSellLife[] =
	"FXBishopSellLife(AttackerName, AttackedName) : Efecto de dar vida (de Attacker [Bishop ó NULL] a Att"
	"acked[Usuario]).";

char str_PyDoc_SFX_MoneyTransfer[] =
	"FXMoneyTransfer(EntityFrom, EntityTo, Time) : Transferencia de dinero";

char str_PyDoc_SFX_EmbeddedSet[] =
	"FXEmbeddedSet(EntityName, FXType) : Asigna el controlador de efectos embedidos.";

