/*
 * Python module SFX (method table SFX_methods, rva 0x42d060): effects. Original source: PyInterface/SFXPy.cpp.
 * Entities are given by name; a missing entity or one of the wrong type fails a CHECK like in the original.
 */
#include "scrap/fx.h"
#include "scrap/global.h"
#include "scrap/python.h"

static PyObject *ReturnNone(void) {
	Py_INCREF(Py_None);
	return Py_None;
}

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
	return ReturnNone();
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
	return ReturnNone();
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
	return ReturnNone();
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
	return ReturnNone();
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
	return ReturnNone();
}

/* FXEmbeddedSet(EntityName, FXType) : Asigna el controlador de efectos embedidos */
PyObject *SFX_EmbeddedSet(PyObject *self, PyObject *args) {
	char *entityName, *fxType;

	(void)self;
	if (!PyArg_ParseTuple(args, "ss", &entityName, &fxType)) {
		return NULL;
	}
	cEntity *vEntity = World_FindEntity(entityName);
	CHECK(vEntity);
	vEntity->vftable->SetEmbeddedFX(vEntity, fxType);
	return ReturnNone();
}
