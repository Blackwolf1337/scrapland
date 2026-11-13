/*
 * Python module SLogic (function table SLogic_methods): game logic of characters, ships and the
 * domination mode. Original source: PyInterface/SLogic.cpp.
 */
#include <string.h>

#include "scrap/global.h"
#include "scrap/logic.h"
#include "scrap/scriptapi.h"
#include "scrap/vehicleentity.h"

/* the four traffic lists GetNearestShip searches (pointer table) */
static const char *const shipLists[] = { "Traffic", "EnemiesTraffic", "FightingTraffic", "NormalTraffic" };

/*
 * void SetCharState(entityName, state, entityTargetName) : Pone a una entidad en un estado de logica y un target
 * determinados. The original returns NULL without a Python exception (SystemError in Python) for every failure. A
 * missing entity still does so: tools/test/golden records it. differs from the original: an entity that is no
 * WalkChar raises a TypeError, a WalkChar without a BaseLogic a ValueError.
 */
API_FN(SLogic, SetCharState) {
	const char *name, *targetName;
	int state;
	if (!Mode_Parse(a, n, "sis", &name, &state, &targetName)) {
		return 0;
	}
	cEntity *e = World_FindEntity(name);
	if (!e) {
		return 0;
	}
	if (!Entity_IsType(e, TYEN_WALKCHAR)) {
		return Native_Raise(NE_TYPE, "SLogic.SetCharState: '%.128s' is no WalkChar", name);
	}
	cEntity *target = World_FindEntity(targetName);
	void *actCtrl = WithLife_GetActCtrl(e);
	CHECK(actCtrl);
	cLocalLogic *logic = Logic_GetLocalLogic(e);
	if (!logic) {
		return Native_Raise(NE_VALUE, "SLogic.SetCharState: '%.128s' has no BaseLogic", name);
	}
	Logic_SetState(logic, state, target);
	return 1;
}

/* void SendSentinelToWatch(Pos) : Envia a un centinela a inspeccionar una posicion */
API_FN(SLogic, SendSentinelToWatch) {
	vec3_t pos;
	if (!Mode_Parse(a, n, "(fff)", &pos.x, &pos.y, &pos.z)) {
		return 0;
	}
	AICtrl_SendSentinelToWatch(&g_pWorld->logic.aiCtrl, &pos);
	return 1;
}

/* void GearAttack(Entity) : Envia a un Gear a atacar a una entidad (NULL if there is none of that name) */
API_FN(SLogic, GearAttack) {
	const char *name;
	if (!Mode_Parse(a, n, "s", &name)) {
		return 0;
	}
	AICtrl_GearAttack(&g_pWorld->logic.aiCtrl, World_FindEntity(name), 0);
	return 1;
}

/* int IsEnemyActive() : Indica si hay un enemigo activo, mirando la list de enemigos y el tráfico. */
API_FN(SLogic, IsEnemyActive) {
	if (!Mode_Parse(a, n, "")) {
		return 0;
	}
	return Api_Build(out, "i", cEntityList_GetFirst(cWorld_GetList(g_pWorld, "EnemiesTraffic")) ||
		cEntityList_GetFirst(cWorld_GetList(g_pWorld, "Enemies")));
}

/*
 * Name GetNearestShip(pos[,exclude]) : Cicla por varias lists y devuelve la nave más cercana a la posición.
 * As compiled: the first car of a list counts as result before any distance check, and a list entry that is
 * not a Car is used through a NULL pointer (crash in the original as here).
 */
API_FN(SLogic, GetNearestShip) {
	vec3_t pos;
	const char *exclude = "";
	if (!Mode_Parse(a, n, "(fff)|s", &pos.x, &pos.y, &pos.z, &exclude)) {
		return 0;
	}
	const char *nearest = "";
	float best = 3.4028234663852886e+38f; /* FLT_MAX */
	for (size_t i = 0; i < sizeof(shipLists) / sizeof(shipLists[0]); i++) {
		cEntityList *list = cWorld_GetList(g_pWorld, shipLists[i]);
		cEntity *first = cEntityList_GetFirst(list);
		if (!first || !Entity_IsType(first, TYEN_CAR)) {
			continue;
		}
		nearest = first->name;
		for (DListNode *node = list->head->data ? list->head : NULL; node;
				node = node->next->data ? node->next : NULL) {
			cEntity *e = node->data;
			if (e && !Entity_IsType(e, TYEN_CAR)) {
				e = NULL;
			}
			if (!strcmp(e->name, exclude) || ((cVehicleEntity *)e)->wl.life <= 0) {
				continue;
			}
			vec3_t tmp;
			const vec3_t *p = Entity_GetPos(e, &tmp);
			float dx = p->x - pos.x, dy = p->y - pos.y, dz = p->z - pos.z;
			float d = dx * dx + dy * dy + dz * dz;
			if (best > d) {
				nearest = e->name;
				best = d;
			}
		}
	}
	return Api_Build(out, "s", nearest);
}

/* void SetOnFloor(Entity) : Pone una entidad en el suelo; 1 if it worked */
API_FN(SLogic, SetOnFloor) {
	const char *name;
	return Mode_Parse(a, n, "s", &name) && Api_Build(out, "i", Logic_SetOnFloor(World_FindEntity(name)) != 0);
}

/* void Flash((x,y,z),radius) : flash at a position */
API_FN(SLogic, Flash) {
	vec3_t pos;
	float radius;
	return Mode_Parse(a, n, "(fff)f", &pos.x, &pos.y, &pos.z, &radius) &&
		Api_Build(out, "i", Logic_Flash(&pos, radius, -1));
}

/* void SetShipToRegenerate(shipName, regSpeed, regEndSpeed): Pone una nave a regenerarse. Not a Car: NULL ship. */
API_FN(SLogic, SetShipToRegenerate) {
	const char *name;
	float speed, endSpeed;
	if (!Mode_Parse(a, n, "sff", &name, &speed, &endSpeed)) {
		return 0;
	}
	cEntity *e = World_FindEntity(name);
	if (e) {
		ShipRegen_Set(&g_pWorld->logic.shipRegen, Entity_IsType(e, TYEN_CAR) ? e : NULL, speed, endSpeed);
	}
	return 1;
}

/*
 * void SetDominationZones(zonesList): Establece la list de las zones de dominación (WithLife entities by name). A list
 * that holds a value of no native kind (an NV_OBJ list) has an item that is no text: its CHECK fails before any zone
 * is added (the original added the zones before that item first)
 */
API_FN(SLogic, SetDominationZones) {
	const nativeVal_t *zones;
	if (!Mode_Parse(a, n, "O", &zones)) {
		return 0;
	}
	CHECK(zones->type == NV_LIST && "El argumento pasado no es una lista");
	CHECK(zones->n > 0 && "El tamaño de la lista es inválido");
	for (int i = 0; i < zones->n; i++) {
		const nativeVal_t *elem = &zones->items[i];
		CHECK(elem->type == NV_STR && "El elemento no es un string");
		cEntity *e = World_FindEntity(Native_Text(elem));
		CHECK(e && Entity_IsType(e, TYEN_WITH_LIFE) && "La entidad no es un 'WithLifeEntity'");
		Domination_AddZone(e);
	}
	return 1;
}

/* void ChangeZoneState(zoneId, state): Pone el estado de una zona de dominación */
API_FN(SLogic, ChangeZoneState) {
	int zone, state;
	if (!Mode_Parse(a, n, "ii", &zone, &state)) {
		return 0;
	}
	Domination_SetZoneState(zone, state);
	return 1;
}

/* void ChangeBatonState(pos, state): Actualiza el estado del testigo en el modo dominación */
API_FN(SLogic, ChangeBatonState) {
	vec3_t pos;
	int state;
	if (!Mode_Parse(a, n, "(fff)i", &pos.x, &pos.y, &pos.z, &state)) {
		return 0;
	}
	Domination_SetBatonState(&pos, state);
	return 1;
}

/* void UpdateTauntEndTime(time): Tiempo final del taunt. The original parses no arguments at all. */
API_FN(SLogic, UpdateTauntEndTime) {
	Taunts_UpdateEndTime(&g_pWorld->logic);
	return 1;
}

static char str_PyDoc_SLogic_ChangeBatonState[];
static char str_PyDoc_SLogic_ChangeZoneState[];
static char str_PyDoc_SLogic_Flash[];
static char str_PyDoc_SLogic_GearAttack[72];
static char str_PyDoc_SLogic_GetNearestShip[];
static char str_PyDoc_SLogic_IsEnemyActive[];
static char str_PyDoc_SLogic_SendSentinelToWatch[88];
static char str_PyDoc_SLogic_SetCharState[];
static char str_PyDoc_SLogic_SetDominationZones[136];
static char str_PyDoc_SLogic_SetOnFloor[60];
static char str_PyDoc_SLogic_SetShipToRegenerate[];
static char str_PyDoc_SLogic_UpdateTauntEndTime[60];
static const portFn_t SLogic_methods[] = {
	PORT_FND(SendSentinelToWatch, SLogic_SendSentinelToWatch, str_PyDoc_SLogic_SendSentinelToWatch),
	PORT_FND(GearAttack, SLogic_GearAttack, str_PyDoc_SLogic_GearAttack),
	PORT_FND(SetCharState, SLogic_SetCharState, str_PyDoc_SLogic_SetCharState),
	PORT_FND(IsEnemyActive, SLogic_IsEnemyActive, str_PyDoc_SLogic_IsEnemyActive),
	PORT_FND(GetNearestShip, SLogic_GetNearestShip, str_PyDoc_SLogic_GetNearestShip),
	PORT_FND(SetOnFloor, SLogic_SetOnFloor, str_PyDoc_SLogic_SetOnFloor),
	PORT_FND(Flash, SLogic_Flash, str_PyDoc_SLogic_Flash),
	PORT_FND(SetShipToRegenerate, SLogic_SetShipToRegenerate, str_PyDoc_SLogic_SetShipToRegenerate),
	PORT_FND(SetDominationZones, SLogic_SetDominationZones, str_PyDoc_SLogic_SetDominationZones),
	PORT_FND(ChangeZoneState, SLogic_ChangeZoneState, str_PyDoc_SLogic_ChangeZoneState),
	PORT_FND(ChangeBatonState, SLogic_ChangeBatonState, str_PyDoc_SLogic_ChangeBatonState),
	PORT_FND(UpdateTauntEndTime, SLogic_UpdateTauntEndTime, str_PyDoc_SLogic_UpdateTauntEndTime),
	PORT_FNEND
};

SD_PY_MODULE(SLogic, 0x42dd20)


static char str_PyDoc_SLogic_SetCharState[] =
	"void SetCharState(entityName, state, entityTargetName) : Pone a una entidad en un estado de logica y"
	" un target determinados";

static char str_PyDoc_SLogic_Flash[] = "void Flash((x,y,z),radius) : Pone una entidad en el suelo";

static char str_PyDoc_SLogic_SendSentinelToWatch[88] =
	"void SendSentinelToWatch(Pos) : Envia a un centinela a inspeccionar una posicion";

static char str_PyDoc_SLogic_GearAttack[72] =
	"void GearAttack(Entity) : Envia a un Gear a atacar a una entidad";

static char str_PyDoc_SLogic_IsEnemyActive[] =
	"int IsEnemyActive() : Indica si hay un enemigo activo, mirando el contenido de la lista de enemigos "
	"y en el tráfico.";

static char str_PyDoc_SLogic_GetNearestShip[] =
	"Name GetNearestShip(pos) : Cicla por varias listas y devuelve la nave más cercana a la posición indi"
	"cada";

static char str_PyDoc_SLogic_SetOnFloor[60] = "void SetOnFloor(Entity) : Pone una entidad en el suelo";

static char str_PyDoc_SLogic_SetShipToRegenerate[] =
	"void SetShipToRegenerate(shipName, regSpeed,regEndSpeed): Pone una nave a regenerarse";

static char str_PyDoc_SLogic_SetDominationZones[136] =
	"void SetDominationZones(zonesList): Establece la lista de posiciones de las zonas de dominación en l"
	"a super apuesta de dominación";

static char str_PyDoc_SLogic_ChangeZoneState[] =
	"void ChangeZoneState(zoneId, state): Pone el estado de una zona de dominación";

static char str_PyDoc_SLogic_ChangeBatonState[] =
	"void ChangeBatonState(pos, state): Actualiza el estado del testigo en el modo dominación";

static char str_PyDoc_SLogic_UpdateTauntEndTime[60] = "void UpdateTauntEndTime(time): Tiempo final del taunt";

PORTS(Engine_SLogic)
