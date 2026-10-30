/*
 * Logic/Logic.cpp of the game: cLogicControl (include/scrap/logic.h), the logic control of the world at
 * g_pWorld->taunts. It owns the mode objects (parking zones, items, ship regeneration, the police and enemy ships
 * of the outdoor maps, the multiplayer modes, the AI control of the indoor police, the nurses, the GenChar group,
 * the gatekeepers), makes their lists at the reset and updates them every frame; the alarm grows by the highest
 * level of the round. Every 0.5 s the taunts (outdoor maps) and the warnings play. The entities of a level are put
 * on the floor here.
 */
#include "scrap/chartype.h"
#include "scrap/cstring.h"
#include "scrap/scrapcollision.h"
#include "scrap/timer.h"

#define LOGIC_SETORIGINFUNC 0x151ce0
#define g_NotCleanableByGearDef (GAME_ARRAY_IN(cString, GVar_NotCleanableByGear, 20, 0x44cba4))
#define Car_B43a(e) (*(byte *)((byte *)(e) + 0x43a))
#define Car_Radius(e) (*(float *)((byte *)(e) + 0x3a0))

/* the x87 code of the original: a float plus a float, rounded by the x87 control word, then to float */
static inline float Logic_X87Add(float a, float b) {
	long double r;
	__asm__("fadds %2" : "=t"(r) : "0"((long double)a), "m"(b));
	return (float)r;
}

void *__thiscall Logic_ctor(void *logic) {
	cLogicControl *self = logic;

	self->vftable = CLOGICCONTROL_VFTABLE;
	self->warned = 0;
	self->warnTime = 0.0f;
	self->timer = 0.0f;
	self->tauntTime = 0.0f;
	self->talkEnd = 0.0f;
	Parking_ctor(&self->parking);
	cInfItem_ctor(&self->items);
	ShipRegen_ctor(&self->shipRegen);
	cExtPolice_ctor(&self->extPolice);
	cEnemies_ctor(&self->enemies);
	cFriends_ctor(&self->friends);
	cShipGroup_ctor(&self->mosquitos, "Mosquitos");
	cShipGroup_ctor(&self->madHunters, "MadHunters");
	cDeathMatch_ctor(&self->deathMatch);
	cCTFFriends_ctor(&self->ctfFriends);
	cCTFEnemies_ctor(&self->ctfEnemies);
	cDominationFriendsLogic_ctor(&self->dominFriends);
	cDominationEnemiesLogic_ctor(&self->dominEnemies);
	AICtrl_ctor(&self->aiCtrl);
	cEnemiesTraffic_ctor(&self->enemiesTraffic);
	cNurses_ctor(&self->nurses);
	cGenCharGroup_ctor(&self->genChars);
	cGateKeepers_ctor(&self->gateKeepers);
	cString_Assign((cString *)&g_NotCleanableByGear, g_NotCleanableByGearDef);
	g_SetOriginFunc = GAME_FUNC(Logic_SetOriginCallback, LOGIC_SETORIGINFUNC);
	return self;
}

void __thiscall Logic_dtor(void *logic) {
	cLogicControl *self = logic;

	self->vftable = CLOGICCONTROL_VFTABLE;
	Logic_NopDtor(&self->gateKeepers);
	cGenCharGroup_dtor(&self->genChars);
	Logic_NopDtor(&self->nurses);
	cEnemiesTraffic_dtor(&self->enemiesTraffic);
	AICtrl_dtor(&self->aiCtrl);
	cDominationEnemiesLogic_dtor(&self->dominEnemies);
	cDominationFriendsLogic_dtor(&self->dominFriends);
	cCTFEnemies_dtor(&self->ctfEnemies);
	cCTFFriends_dtor(&self->ctfFriends);
	cDeathMatch_dtor();
	cShipGroup_dtor(&self->madHunters);
	cShipGroup_dtor(&self->mosquitos);
	Logic_NopDtor(&self->friends);
	Logic_NopDtor(&self->enemies);
	Logic_NopDtor(&self->extPolice);
	ShipRegen_dtor(&self->shipRegen);
	cInfItem_dtor(&self->items);
	Parking_dtor(&self->parking);
}

cLogicControl *__thiscall Logic_Delete(cLogicControl *self, byte flags) {
	Logic_dtor(self);
	if (flags & 1) {
		operator_delete_sized(self, sizeof(*self));
	}
	return self;
}

/* every 0.5 s: the taunts (outdoor maps, TimeBetweeenTaunts), the warnings (the first at once, then after
 * TimeBetweeenWarnings); the talk ends when the player sits in a car */
void __thiscall Logic_UpdateTaunts(cLogicControl *self, float dt) {
	float t0 = dt + self->timer;
	float t1 = dt + self->tauntTime;
	float t2 = dt + self->warnTime;

	self->timer = t0;
	self->tauntTime = t1;
	self->warnTime = t2;
	if (t0 >= 0.5f) {
		self->timer = t0 - 0.5f;
		if (g_IsMapOutDoor == 1 && t1 >= g_TimeBetweeenTaunts) {
			Taunts_Think(dt);
			self->tauntTime = 0.0f;
		}
		if (!self->warned) {
			self->warned = Taunts_Warnings(dt);
			self->warnTime = 0.0f;
		} else if (self->warnTime >= g_TimeBetweeenWarnings && Taunts_Warnings(dt)) {
			self->warnTime = 0.0f;
		}
	}
	cEntity *usr = ChType_Usr0();
	if (usr && usr->vftable->IsType(usr, TYEN_CAR) && Car_B43a(usr)) {
		float now = Timer_GetTime(&g_pWorld->timer);
		if (self->talkEnd > now) {
			g_RemainTalkTime = 0.0f;
			self->talkEnd = 0.0f;
		}
	}
}

/* g_SetOriginFunc: the logic of a new entity takes its position as the origin */
byte Logic_SetOriginCallback(cEntity *e) {
	cLocalLogic *l = Logic_GetLocalLogic(e);

	if (l) {
		l->vftable->SetOriginFrom(l, e);
	}
	return 0;
}

void __thiscall Logic_Reset(void *logic) {
	cLogicControl *self = logic;

	cExtPolice_Init(&self->extPolice);
	cEnemies_Init(&self->enemies);
	cFriends_Init(&self->friends);
	cShipGroup_Init(&self->mosquitos);
	cShipGroup_Init(&self->madHunters);
	AICtrl_Init(&self->aiCtrl);
	cEnemiesTraffic_Init(&self->enemiesTraffic);
	cDeathMatch_Init(&self->deathMatch);
	cCTFFriends_Init(&self->ctfFriends);
	cCTFEnemies_Init(&self->ctfEnemies);
	self->dominEnemies.base.vftable->Init(&self->dominEnemies.base);
	self->dominFriends.base.vftable->Init(&self->dominFriends.base);
	cNurses_Init(&self->nurses);
	cGenCharGroup_Init(&self->genChars);
	cGateKeepers_Init(&self->gateKeepers);
}

void __thiscall Logic_Update(void *logic, float dt) {
	cLogicControl *self = logic;
	int level = -1;

	Parking_Update(&self->parking, dt);
	ShipRegen_Update(&self->shipRegen, dt);
	cExtPolice_Update(&self->extPolice, dt);
	cEnemies_Update(&self->enemies, dt);
	cFriends_Update(&self->friends, dt);
	cShipGroup_Update(&self->mosquitos, dt);
	cShipGroup_Update(&self->madHunters, dt);
	cDeathMatch_Update(&self->deathMatch, dt);
	cCTFFriends_Update(&self->ctfFriends, dt);
	cCTFEnemies_Update(&self->ctfEnemies, dt);
	self->dominEnemies.base.vftable->Update(&self->dominEnemies.base, dt);
	self->dominFriends.base.vftable->Update(&self->dominFriends.base, dt);
	AICtrl_Update(&self->aiCtrl, dt);
	cNurses_Update(&self->nurses, dt);
	cEnemiesTraffic_Update(&self->enemiesTraffic, dt);
	cGenCharGroup_Update(&self->genChars, dt);
	cGateKeepers_Update(&self->gateKeepers, dt);
	if (self->aiCtrl.level > level) {
		level = self->aiCtrl.level;
	}
	if (cGenCharGroup_GetLevel(&self->genChars) > level) {
		level = cGenCharGroup_GetLevel(&self->genChars);
	}
	if (self->extPolice.raised > level) {
		level = self->extPolice.raised;
	}
	Alarm_SetGrow(g_pWorld->alarm, level);
	Logic_UpdateTaunts(self, dt);
}

void __thiscall Taunts_UpdateEndTime(void *taunts) {
	cLogicControl *self = taunts;

	self->talkEnd = Logic_X87Add(Timer_GetTime(&g_pWorld->timer), g_RemainTalkTime);
}

/* a character on the floor below it (floorDist above the hit), 0 and an error without a floor */
byte Logic_CharOnFloor(cEntity *e) {
	vec3_t t1, t2, to, hit;
	const vec3_t *p = e->vftable->GetPos(e, &t1);

	to.z = p->z + 0.0f;
	to.y = p->y - 10000.0f;
	to.x = p->x + 0.0f;
	byte ok = Engine_TestLine(e->vftable->GetPos(e, &t2), &to, &hit, NULL);
	if (!ok) {
		Con_Bluef("B R U T A L    E R R O R : %s was created in an invalid position.\n", e->name);
		return 0;
	}
	to.x = hit.x + 0.0f;
	to.y = hit.y + WChar(e)->floorDist;
	to.z = hit.z + 0.0f;
	e->vftable->SetPos(e, &to);
	return ok;
}

/* a parked ship on the floor below it (its radius above the hit) with only its y angle */
byte Logic_CarOnFloor(cEntity *e) {
	vec3_t t1, t2, to, hit;
	const vec3_t *p = e->vftable->GetPos(e, &t1);

	to.z = p->z + 0.0f;
	to.y = p->y - 10000.0f;
	to.x = p->x + 0.0f;
	byte ok = Engine_TestLine(e->vftable->GetPos(e, &t2), &to, &hit, NULL);
	if (!ok) {
		Con_Bluef("B R U T A L    E R R O R : Parked Ship %s was created in an invalid position.\n", e->name);
		return 0;
	}
	float r = Car_Radius(e);
	float y;
	if (Obj_Vtbl(e)->GetRadius(e) > r) {
		y = Obj_Vtbl(e)->GetRadius(e);
	} else {
		y = Car_Radius(e);
	}
	to.y = y + hit.y;
	to.x = hit.x + 0.0f;
	to.z = hit.z + 0.0f;
	e->vftable->SetPos(e, &to);
	const vec3_t *a = Obj_Vtbl(e)->GetAng(e, &t2);
	to.y = a->y;
	to.x = 0.0f;
	to.z = 0.0f;
	Obj_Vtbl(e)->SetAng(e, &to);
	return ok;
}

byte Logic_SetOnFloor(cEntity *e) {
	if (e) {
		if (e->vftable->IsType(e, TYEN_WALKCHAR)) {
			Logic_CharOnFloor(e);
			return 0;
		}
		if (e->vftable->IsType(e, TYEN_CAR)) {
			Logic_CarOnFloor(e);
		}
	}
	return 0;
}
