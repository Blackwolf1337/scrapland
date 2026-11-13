/*
 * World/World.cpp of the game: cWorld, the world of the game entities, see include/scrap/world.h.
 */
#include <string.h>

#include "scrap/ai.h"
#include "scrap/app.h"
#include "scrap/camcontrol.h"
#include "scrap/console.h"
#include "scrap/crt.h"
#include "scrap/hashtable.h"
#include "scrap/input.h"
#include "scrap/itementity.h"
#include "scrap/material.h"
#include "scrap/players.h"
#include "scrap/missileentity.h"
#include "scrap/scheduler.h"
#include "scrap/scrapcollision.h"
#include "scrap/scrapmain.h"
#include "scrap/scrapmodel.h"
#include "scrap/scrapworld.h"
#include "scrap/sound.h"
#include "scrap/vehicleentity.h"
#include "scrap/walkcharentity.h"
#include "scrap/withlifeentity.h"
#include "scrap/world.h"
#include "scrap/staticinit.h"

#define g_MaxRadObj (GVar_Max_Rad_Obj.value.f)		/* Max_Rad_Obj, the margin of the grid queries */
#define g_GridScaleX (GVar_GridScaleX.value.f)
#define g_GridScaleY (GVar_GridScaleY.value.f)
#define g_GridScaleZ (GVar_GridScaleZ.value.f)
#define g_GameSkill (GVar_GameSkill.value.i)
#define g_GameSkillDef (GVar_GameSkill.def.i)
#define g_PriorityWeapon (((cString *)&GVar_PriorityWeapon.value))	/* PriorityWeapon, its default at +4 */
#define g_MainMissionEvent (((cPy_Function *)&GVar_MainMissionEvent.func))
#define g_EscapeEvent (((cPy_Function *)&GVar_EscapeEvent.func))
#define g_ChatEvent (((cPy_Function *)&GVar_ChatEvent.func))
#define g_QChatEvent1 (((cPy_Function *)&GVar_QChatEvent1.func))
#define g_QChatEvent2 (((cPy_Function *)&GVar_QChatEvent2.func))
#define g_QChatEvent3 (((cPy_Function *)&GVar_QChatEvent3.func))
#define g_QChatEvent4 (((cPy_Function *)&GVar_QChatEvent4.func))
#define g_QChatEvent5 (((cPy_Function *)&GVar_QChatEvent5.func))
#define g_QChatEvent6 (((cPy_Function *)&GVar_QChatEvent6.func))
static cGVarFunction GVar_QChatEvent6;
static cGVarFunction GVar_QChatEvent5;
static cGVarFunction GVar_QChatEvent4;
static cGVarFunction GVar_QChatEvent3;
static cGVarFunction GVar_QChatEvent2;
static cGVarFunction GVar_QChatEvent1;
static cGVar GVar_PriorityWeapon;
static cGVar GVar_Max_Rad_Obj;
static cGVarFunction GVar_MainMissionEvent;
static cGVar GVar_GridScaleZ;
static cGVar GVar_GridScaleY;
static cGVar GVar_GridScaleX;
static cGVar GVar_GameSkill;
static cGVarFunction GVar_EscapeEvent;
static cGVarFunction GVar_ChatEvent;
#define PI_F 3.14159265f
#define FLT_MAX_F 3.40282347e+38f

typedef TABLE(cAmmoInfo) cAmmoTable;
typedef TABLE(cWeapInfo) cWeapTable;

/* the listener of a view: position, orientation and velocity */
#define Snd_SetListener(vt) ((vt)->SetListener)

static cEntity *World_Lookup(cWorld *w, const char *name) {
	cPtrItem *it = PtrTable_Find(&w->entities, name, HashTable_Slot(&w->entities, name));

	return it ? it->value : NULL;
}

static int World_IsIgnoredInline(cWorld *self, cEntity *e) {
	for (int i = 0; i < 4; i++) {
		if (!self->ignore[i]) {
			break;
		}
		if (self->ignore[i] == e) {
			return 1;
		}
	}
	return 0;
}

/* the box of a sphere query: the margin of the grid on the radius */
static void World_SphereBox(const vec3_t *c, float radius, vec3_t *lo, vec3_t *hi) {
	float r = g_MaxRadObj + radius;

	hi->x = c->x + r;
	lo->x = c->x - r;
	hi->y = c->y + r;
	lo->y = c->y - r;
	hi->z = c->z + r;
	lo->z = c->z - r;
}

/*
 * The "Car" in view of self nearest to its view axis, within range and not behind a wall; with an AI only its target.
 * mode 0: not neutral (team 3), 1: an enemy team, 2: any. *outDot gets the cosine, *outDist the distance.
 */
cEntity *Entity_FindInView(cEntity *self, float range, float *outDot, float *outDist, int mode, float minDot) {
	cEntity *best = NULL;
	float bestDist = 0.0f;
	float range2 = range * range;
	vec3_t pos, ang, tmp;

	Obj_GetCamera(self, &pos, &ang);
	ang.x = ang.x + PI_F;
	byte hasAI = Entity_GetAI(self) != NULL;
	cEntity *e = hasAI ? AIMove_GetTargetEnt(Entity_GetAI(self)) : cWorld_GetFirstEntity(g_pWorld);
	if (!e) {
		return NULL;
	}
	for (;;) {
		if (e->active && e != self && Entity_IsType(e, TYEN_CAR)) {
			int take;
			if (hasAI || mode == 2) {
				take = 1;
			} else if (mode == 1) {
				int et = WLife(e)->arrow.tgType, st = WLife(self)->arrow.tgType;
				take = et != 3 && st != 3 && (et != st || et == 4 || st == 4);
			} else {
				take = mode == 0 && WLife(e)->arrow.tgType != 3;
			}
			if (take) {
				vec3_t tp = *Entity_GetPos(e, &tmp);
				float dx = tp.x - pos.x;
				float dy = tp.y - pos.y;
				float dz = tp.z - pos.z;
				float d2 = dy * dy + dx * dx + dz * dz;
				if (range2 > d2) {
					float dist = (float)__builtin_sqrt((double)d2);
					float inv = 1.0f / dist;
					float sx, cx, sy, cy;
					float nx = dx * inv, ny = dy * inv, nz = dz * inv;
					Float_SinCos(-ang.x, &sx, &cx);
					Float_SinCos(ang.y + PI_F, &sy, &cy);
					float dot = (nz * cy + nx * sy) * cx - ny * sx;
					if (dot > minDot && !Engine_TestLine(&pos, &tp, &tp, NULL)) {
						minDot = dot;
						best = e;
						bestDist = dist;
					}
				}
			}
		}
		if (hasAI) {
			break;
		}
		e = cEntity_Next(e);
		if (!e) {
			break;
		}
	}
	if (best) {
		if (outDist) {
			*outDist = bestDist;
		}
		if (outDot) {
			*outDot = minDot;
		}
	}
	return best;
}

byte Team_IsFriend(int a, int b) {
	return a == b && a != 4 && a != 3;
}

byte Team_IsEnemy(int a, int b) {
	return a != 3 && b != 3 && (a != b || a == 4 || b == 4);
}

static cAmmoInfo *cAmmoInfo_ctor(cAmmoInfo *self) {
	cString_ctor(&self->name);
	cPy_Function_ctor(&self->onPress);
	cPy_Function_ctor(&self->onRelease);
	cPy_Function_ctor(&self->onFire);
	cPy_Function_ctor(&self->onHit);
	self->max = 0.0f;
	self->charger = 0.0f;
	return self;
}

static cWeapInfo *cWeapInfo_ctor(cWeapInfo *self) {
	cString_ctor(&self->name);
	cPy_Function_ctor(&self->onPress);
	cPy_Function_ctor(&self->onRelease);
	cPy_Function_ctor(&self->onFire);
	cPy_Function_ctor(&self->onHit);
	cPy_Function_ctor(&self->onStart);
	cPy_Function_ctor(&self->onEnd);
	cPy_Function_ctor(&self->setFx);
	self->ammo = 0;
	self->next = 0;
	self->prev = 0;
	self->minAmmo = 0.0f;
	memset(self->upgrade, 0, sizeof(self->upgrade));
	return self;
}

static void cAmmoInfo_dtor(cAmmoInfo *self) {
	cPy_Function_dtor(&self->onHit);
	cPy_Function_dtor(&self->onFire);
	cPy_Function_dtor(&self->onRelease);
	cPy_Function_dtor(&self->onPress);
	cString_dtor(&self->name);
}

static void cWeapInfo_dtor(cWeapInfo *self) {
	cPy_Function_dtor(&self->setFx);
	cPy_Function_dtor(&self->onEnd);
	cPy_Function_dtor(&self->onStart);
	cPy_Function_dtor(&self->onHit);
	cPy_Function_dtor(&self->onFire);
	cPy_Function_dtor(&self->onRelease);
	cPy_Function_dtor(&self->onPress);
	cString_dtor(&self->name);
}

/* the Global/Table.h Init of the two instances: new T[num] with the constructors */
void AmmoTable_Init(void *table, int num) {
	TABLE(cAmmoInfo) *t = table;

	CHECK(!t->items && num > 0);
	t->count = num;
	t->items = New_Objects(num, sizeof(cAmmoInfo), (void *)cAmmoInfo_ctor, (void *)cAmmoInfo_dtor);
	CHECK(t->items);
}

void WeapTable_Init(void *table, int num) {
	TABLE(cWeapInfo) *t = table;

	CHECK(!t->items && num > 0);
	t->count = num;
	t->items = New_Objects(num, sizeof(cWeapInfo), (void *)cWeapInfo_ctor, (void *)cWeapInfo_dtor);
	CHECK(t->items);
}

int cAmmoInfo_GetF(cAmmoInfo *ammo, const char *var, float *value) {
	if (!crt_stricmp(var, "Max")) {
		*value = ammo->max;
		return 1;
	}
	if (!crt_stricmp(var, "Charger")) {
		*value = ammo->charger;
		return 1;
	}
	return 0;
}

/* "UpgrdNN": two digits */
static int Weap_IsUpgradeVar(const char *var) {
	return strlen(var) == 7 && var[5] <= '9' && var[5] >= '0' && var[6] <= '9' && var[6] >= '0' &&
		!crt_memicmp(var, "Upgrd", 5);
}

int cWeapInfo_GetF(cWeapInfo *weap, const char *var, float *value) {
	if (!crt_stricmp(var, "MinAmmo")) {
		*value = weap->minAmmo;
		return 1;
	}
	if (!crt_stricmp(var, "iPrev")) {
		*value = (float)weap->prev;
		return 1;
	}
	if (!crt_stricmp(var, "iNext")) {
		*value = (float)weap->next;
		return 1;
	}
	if (!crt_stricmp(var, "iAmmo")) {
		*value = (float)weap->ammo;
		return 1;
	}
	if (Weap_IsUpgradeVar(var)) {
		*value = *(weap->upgrade - 1 + crt_atoi(var + 5)) ? 1.0f : 0.0f;
		return 1;
	}
	return 0;
}

int cAmmoInfo_GetS(cAmmoInfo *ammo, const char *var, char **value) {
	if (!crt_stricmp(var, "Name")) {
		*value = ammo->name;
		return 1;
	}
	if (!crt_stricmp(var, "OnPress")) {
		*value = "OnPress(<EntityName>)";
		return 1;
	}
	if (!crt_stricmp(var, "OnRelease")) {
		*value = "OnRelease(<EntityName>)";
		return 1;
	}
	if (!crt_stricmp(var, "OnFire")) {
		*value = "OnFire(<EntityName>)";
		return 1;
	}
	if (!crt_stricmp(var, "OnHit")) {
		*value = "OnHit(Attacker,HitX,HitY,HitZ,Attacked)";
		return 1;
	}
	return 0;
}

int cWeapInfo_GetS(cWeapInfo *weap, const char *var, char **value) {
	if (!crt_stricmp(var, "Name")) {
		*value = weap->name;
		return 1;
	}
	if (!crt_stricmp(var, "OnPress")) {
		*value = "OnPress(<EntityName>)";
		return 1;
	}
	if (!crt_stricmp(var, "OnRelease")) {
		*value = "OnRelease(<EntityName>)";
		return 1;
	}
	if (!crt_stricmp(var, "OnFire")) {
		*value = "OnFire(<EntityName>)";
		return 1;
	}
	if (!crt_stricmp(var, "OnHit")) {
		*value = "OnHit(Attacker,HitX,HitY,HitZ,Attacked)";
		return 1;
	}
	if (!crt_stricmp(var, "OnStart")) {
		*value = "OnStart(<EntityName>)";
		return 1;
	}
	if (!crt_stricmp(var, "OnEnd")) {
		*value = "OnEnd(<EntityName>)";
		return 1;
	}
	if (!crt_stricmp(var, "SetFx")) {
		*value = "SetFx(<EntityName>)";
		return 1;
	}
	return 0;
}

int cAmmoInfo_SetF(cAmmoInfo *ammo, const char *var, float value) {
	if (!crt_stricmp(var, "Max")) {
		ammo->max = value;
		return 1;
	}
	if (!crt_stricmp(var, "Charger")) {
		ammo->charger = value;
		return 1;
	}
	return 0;
}

int cWeapInfo_SetF(cWeapInfo *weap, const char *var, float value) {
	if (!crt_stricmp(var, "MinAmmo")) {
		weap->minAmmo = value;
		return 1;
	}
	if (!crt_stricmp(var, "iPrev")) {
		weap->prev = (int)value;
		return 1;
	}
	if (!crt_stricmp(var, "iNext")) {
		weap->next = (int)value;
		return 1;
	}
	if (!crt_stricmp(var, "iAmmo")) {
		weap->ammo = (int)value;
		return 1;
	}
	if (Weap_IsUpgradeVar(var)) {
		*(weap->upgrade - 1 + crt_atoi(var + 5)) = value > 0.5f;
		return 1;
	}
	return 0;
}

int cAmmoInfo_SetS(cAmmoInfo *ammo, const char *var, const char *value) {
	if (!crt_stricmp(var, "Name")) {
		cString_Set(&ammo->name, value);
		return 1;
	}
	if (!crt_stricmp(var, "OnPress")) {
		cPy_Function_SetFunc(&ammo->onPress, value);
		return 1;
	}
	if (!crt_stricmp(var, "OnRelease")) {
		cPy_Function_SetFunc(&ammo->onRelease, value);
		return 1;
	}
	if (!crt_stricmp(var, "OnFire")) {
		cPy_Function_SetFunc(&ammo->onFire, value);
		return 1;
	}
	if (!crt_stricmp(var, "OnHit")) {
		cPy_Function_SetFunc(&ammo->onHit, value);
		return 1;
	}
	return 0;
}

int cWeapInfo_SetS(cWeapInfo *weap, const char *var, const char *value) {
	if (!crt_stricmp(var, "Name")) {
		cString_Set(&weap->name, value);
		return 1;
	}
	if (!crt_stricmp(var, "OnPress")) {
		cPy_Function_SetFunc(&weap->onPress, value);
		return 1;
	}
	if (!crt_stricmp(var, "OnRelease")) {
		cPy_Function_SetFunc(&weap->onRelease, value);
		return 1;
	}
	if (!crt_stricmp(var, "OnFire")) {
		cPy_Function_SetFunc(&weap->onFire, value);
		return 1;
	}
	if (!crt_stricmp(var, "OnHit")) {
		cPy_Function_SetFunc(&weap->onHit, value);
		return 1;
	}
	if (!crt_stricmp(var, "OnStart")) {
		cPy_Function_SetFunc(&weap->onStart, value);
		return 1;
	}
	if (!crt_stricmp(var, "OnEnd")) {
		cPy_Function_SetFunc(&weap->onEnd, value);
		return 1;
	}
	if (!crt_stricmp(var, "SetFx")) {
		cPy_Function_SetFunc(&weap->setFx, value);
		return 1;
	}
	return 0;
}

cWorld *cWorld_ctor(cWorld *self) {
	cScheduledFuncStack *sched = &self->scheduler;
	box_t box;
	vec3_t cell;

	HashTable_ctor(&self->entities, 0x1c9, 1);
	Nto1Node_Init(&self->entityList);
	Nto1Node_Init(&self->deadList);
	memset(&sched->list, 0, sizeof(sched->list));
	HashTable_ctor(&self->debris, 0x1c9, 1);
	HashTable_ctor(&self->lists, 0x1f, 1);
	cTimer_ctor(&self->timer);
	Floor_ctor(&self->floor);
	Elements_ctor(&self->elements);
	self->weapons.items = NULL;
	self->weapons.count = 0;
	self->ammo.items = NULL;
	self->ammo.count = 0;
	Grid_ctor(&self->grid);
	Alarm_ctor(&self->alarm);
	cStrMoney_ctor(self->strMoney);
	Logic_ctor(&self->logic);
	self->nameCount = 0;
	self->testing = 0;
	self->entityList.target = self;
	self->deadList.target = self;
	World_GetCollideBox(&box);
	cell.x = g_GridScaleX;
	cell.y = g_GridScaleY;
	cell.z = g_GridScaleZ;
	Grid_Init(&self->grid, &box.min, &box.max, &cell);
	self->checkInput = 1;
	self->debrisList = NULL;
	self->firstWeapon = 0;
	memset(self->ignore, 0, sizeof(self->ignore));
	self->curCam = 0;
	for (int i = 0; i < 4; i++) {
		self->camName[i][0] = 0;
		self->camPos[i].x = 0.0f;
		self->camPos[i].y = 0.0f;
		self->camPos[i].z = 0.0f;
	}
	memset(self->owners, 0, sizeof(self->owners));
	g_VehicleEditingMode = 0;
	cPy_Function_Clear(g_OnSleep);
	cPy_Function_Clear(g_OnLifeItemTaken);
	cPy_Function_Clear(g_OnGeneralDamage);
	g_MaxChasingShips = g_MaxChasingShipsDef;
	g_PitchScale = 1.0f;
	g_FreeSpaceMagnet = g_FreeSpaceMagnetDef;
	g_GameSkill = g_GameSkillDef;
	g_FXVehicleFloatingFactor = g_FXVehicleFloatingFactorDef;
	g_SceneMotionBlurValue = 0;
	g_NotShootAI = g_NotShootAIDef;
	SaveVar_Create("Player.NumLives", "99");
	cPy_Function_Clear(g_QChatEvent1);
	cPy_Function_Clear(g_QChatEvent2);
	cPy_Function_Clear(g_QChatEvent3);
	cPy_Function_Clear(g_QChatEvent4);
	cPy_Function_Clear(g_QChatEvent5);
	cPy_Function_Clear(g_QChatEvent6);
	return self;
}

void cWorld_AddIgnore(cWorld *self, cEntity *e) {
	for (int i = 0; i < 4; i++) {
		if (!self->ignore[i]) {
			self->ignore[i] = e;
			return;
		}
	}
	CHECK(0);
}

/* calls func(name, x, y, z, dist) for every entity of mask within radius of the segment, seen from it */
static byte cWorld_LineCallF(cWorld *self, uint32_t mask, const vec3_t *from, const vec3_t *to, float radius,
	cPy_Function *func) {
	byte found = 0;
	vec3_t ext, closest, pos, pos2;

	self->testing = 1;
	CHECK(Grid_IsOk(&self->grid));
	ext.x = ext.y = ext.z = g_MaxRadObj + radius;
	for (cEntity *e = Grid_FirstOnSegment(&self->grid, from, to, &ext); e; e = Grid_NextOnSegment(&self->grid)) {
		if (!(Entity_Mask(e) & mask)) {
			continue;
		}
		float d2 = Dist2_PointSegment(from, to, Entity_GetPos(e, &pos), &closest);
		if (!Entity_TestSphere(e, NULL, &closest, radius, 0)) {
			continue;
		}
		if (Engine_TestLine(&closest, Entity_GetPos(e, &pos2), NULL, NULL)) {
			continue;
		}
		cPy_Function_AddParamS(func, e->name);
		cPy_Function_AddParamF(func, closest.x);
		cPy_Function_AddParamF(func, closest.y);
		cPy_Function_AddParamF(func, closest.z);
		cPy_Function_AddParamF(func, (float)__builtin_sqrt((double)d2));
		cPy_Function_Execute(func, 0);
		found = 1;
	}
	return found;
}

byte cWorld_LineCall(cWorld *self, uint32_t mask, const vec3_t *from, const vec3_t *to, float radius,
	const char *func) {
	cPy_Function f;

	cPy_Function_ctor(&f);
	cPy_Function_SetFunc(&f, func);
	byte r = cWorld_LineCallF(self, mask, from, to, radius, &f);
	cPy_Function_dtor(&f);
	return r;
}

/* action of an action set pressed in this frame, with the index checks of Table<T>::operator[] */

/* the script events of the menu and chat keys of each player */
static void World_InputEvents(void) {
	for (int v = 0; v < 4; v++) {
		if (v == 0) {
			if ((Input_GetActionSet() == ACTSET_RACER && Input_ActionPressed(ACTSET_RACER, RACER_MISSION_INFO, 0)) ||
				(Input_GetActionSet() == ACTSET_WALK && Input_ActionPressed(ACTSET_WALK, WALK_MISSION_INFO, 0))) {
				if (Talk_IsShown()) {
					g_RemainTalkTime = 0.0f;
				} else {
					cPy_Function_AddParamI(g_MainMissionEvent, 0);
					cPy_Function_Execute(g_MainMissionEvent, 0);
				}
			}
		}
		if ((byte)Talk_IsShown() && ((Input_GetActionSet() == ACTSET_RACER && Input_ActionPressed(ACTSET_RACER, RACER_MAIN_MENU, v)) ||
			(Input_GetActionSet() == ACTSET_WALK && Input_ActionPressed(ACTSET_WALK, WALK_MAIN_MENU, v)))) {
			g_RemainTalkTime = 0.0f;
		} else if (v == 0) {
			if ((Input_GetActionSet() == ACTSET_RACER && Input_ActionPressed(ACTSET_RACER, RACER_MAIN_MENU, 0)) ||
				(Input_GetActionSet() == ACTSET_WALK && Input_ActionPressed(ACTSET_WALK, WALK_MAIN_MENU, 0)) ||
				(Input_GetActionSet() == ACTSET_INACTIVE && Input_ActionPressed(ACTSET_INACTIVE, INACTIVE_ABORT, 0))) {
				cPy_Function_AddParamI(g_EscapeEvent, 0);
				cPy_Function_Execute(g_EscapeEvent, 0);
			}
		}
		if (Input_GetActionSet() == ACTSET_RACER) {
			cPy_Function *events[7] = { g_ChatEvent, g_QChatEvent1, g_QChatEvent2, g_QChatEvent3, g_QChatEvent4,
				g_QChatEvent5, g_QChatEvent6 };
			for (int k = 0; k < 7; k++) {
				if (Input_ActionPressed(ACTSET_RACER, RACER_CHAT + k, v)) {
					cPy_Function_AddParamI(events[k], v);
					cPy_Function_Execute(events[k], 0);
					break;
				}
			}
		}
	}
}

cEntity *cWorld_CreateEntity(cWorld *world, const char *className, float x, float y, float z,
	const char *type) {
	void *mem;
	cEntity *e;

	(void)world;
	if (!crt_stricmp(type, "Entity")) {
		mem = operator_new(sizeof(cEntity));
		return mem ? cEntity_ctor(mem, className, x, y, z) : NULL;
	}
	if (!crt_stricmp(type, "CamPath")) {
		mem = operator_new(sizeof(cCamPathEntity));
		return mem ? cCamPathEntity_ctor(mem, className, x, y, z) : NULL;
	}
	if (!crt_stricmp(type, TYEN_WALKCHAR)) {
		mem = operator_new(sizeof(cWalkCharEntity));
		e = mem ? cWalkCharEntity_ctor(mem, className, x, y, z) : NULL;
		Grid_Add(&g_pWorld->grid, e);
		return e;
	}
	if (!crt_stricmp(type, TYEN_CAR)) {
		mem = operator_new(sizeof(cVehicleEntity));
		e = mem ? cVehicleEntity_ctor(mem, className, x, y, z) : NULL;
		Grid_Add(&g_pWorld->grid, e);
		return e;
	}
	if (!crt_stricmp(type, "Cam")) {
		mem = operator_new(sizeof(cCamControl));
		return mem ? cCamEntity_ctor(mem, className, x, y, z) : NULL;
	}
	if (!crt_stricmp(type, "FCam")) {
		mem = operator_new(sizeof(cFollowCamEntity));
		return mem ? cFollowCamEntity_ctor(mem, className, x, y, z) : NULL;
	}
	if (!crt_stricmp(type, "Missile")) {
		mem = operator_new(sizeof(cMissileEntity));
		return mem ? cMissileEntity_ctor(mem, className, x, y, z) : NULL;
	}
	if (!crt_stricmp(type, "item")) {
		mem = operator_new(sizeof(cItemEntity));
		return mem ? cItemEntity_ctor(mem, className, x, y, z) : NULL;
	}
	if (!crt_stricmp(type, TYEN_WITH_LIFE)) {
		mem = operator_new(sizeof(cWithLifeEntity));
		return mem ? cWithLifeEntity_ctor(mem, className, x, y, z) : NULL;
	}
	if (!crt_stricmp(type, "Obj")) {
		mem = operator_new(sizeof(cObjEntity));
		return mem ? cObjEntity_ctor(mem, className, x, y, z) : NULL;
	}
	return NULL;
}

/* the end of a level: scheduled functions, cameras, entities, debris systems and entity lists */
static void cWorld_DeleteLists(cWorld *self);
void cWorld_Clear(cWorld *self) {
	Nto1Node *n;

	Scheduler_Clear(&self->scheduler);
	for (n = self->entityList.next; n->data && n; n = n->next) {
		cEntity *e = n->data;
		if (Entity_IsType(e, "Cam")) {
			CamCtl_SetTarget(e, NULL);
		}
	}
	Camera_SetCurrent(NULL);
	self->curCam = 0;
	for (int i = 0; i < 4; i++) {
		self->camName[i][0] = 0;
	}
	for (;;) {
		n = self->entityList.next;
		if (!n->data || !n) {
			break;
		}
		cEntity *e = n->data;
		if (e) {
			/* out of the name table right after its free, by its pointer: its own destructor may still look itself
			 * up (a missile's death callback), and a later one must not meet freed names (a camera's OnSwitch); the
			 * original left all names in and read freed ones */
			uint32_t slot = HashTable_Slot(&self->entities, e->name);
			Entity_Free(e);
			PtrTable_DeleteValue(&self->entities, slot, e);
		}
	}
	while (self->debrisList) {
		cDebrisSys *d = self->debrisList;
		self->debrisList = d->next;
		if (d) {
			cDebrisSys_dtor(d);
			operator_delete(d);
		}
	}
	cWorld_DeleteLists(self);
	GameModelList_Free();
}

static void AmmoTable_Free(void *table) {
	cAmmoTable *t = table;

	if (t->items) {
		Delete_Objects(t->items, sizeof(cAmmoInfo), (void *)cAmmoInfo_dtor);
		t->items = NULL;
		t->count = 0;
	}
}

static void WeapTable_Free(void *table) {
	cWeapTable *t = table;

	if (t->items) {
		Delete_Objects(t->items, sizeof(cWeapInfo), (void *)cWeapInfo_dtor);
		t->items = NULL;
		t->count = 0;
	}
}

/* the world and everything in it freed (the end of the game) */
void cWorld_Free(cWorld *self) {
	cWorld_Clear(self);
	Logic_dtor(&self->logic);
	Grid_dtor(&self->grid);
	AmmoTable_Free(&self->ammo);
	WeapTable_Free(&self->weapons);
	Elements_dtor(&self->elements);
	Floor_dtor(&self->floor);
	PtrTable_Clear(&self->lists);
	PtrTable_Clear(&self->debris);
	cScheduledFuncStack_dtor(&self->scheduler);
	Nto1Node_Done(&self->deadList);
	Nto1Node_Done(&self->entityList);
	PtrTable_Clear(&self->entities);
	operator_delete(self);
}

static void cWorld_DeleteLists(cWorld *self) {
	HashTable *t = &self->lists;

	for (;;) {
		int n = (int)t->numSlots, i = 0;
		if (n <= 0) {
			return;
		}
		while (!t->slots[i]) {
			if (++i >= n) {
				return;
			}
		}
		cPtrItem *it = (cPtrItem *)t->slots[i];
		if (!it) {
			return;
		}
		cEntityList *list = it->value;
		CHECK(PtrTable_Delete(t, it->key));
		if (list) {
			EntityDList_dtor(&list->ring);
			cPy_Function_dtor(&list->onDamage);
			cPy_Function_dtor(&list->onDeath);
			cString_dtor((cString *)&list->name);
			operator_delete(list);
		}
	}
}

byte cWorld_DeleteDebrisSys(cWorld *world, const char *name) {
	cPtrItem *it = PtrTable_Find(&world->debris, name, HashTable_Slot(&world->debris, name));
	CGameModel model;

	if (!it) {
		return 0;
	}
	cDebrisSys *sys = it->value;
	CHECK(sys);
	PtrTable_Delete(&world->debris, sys->name);
	cDebrisSys *head = world->debrisList, *prev = NULL;
	for (cDebrisSys *d = head; d; prev = d, d = d->next) {
		if (d == sys) {
			if (d == head) {
				world->debrisList = head->next;
			} else {
				prev->next = d->next;
			}
			break;
		}
	}
	/* the model of the system is loaded once more and released with its resources */
	CGameModel_ctor(&model, NULL);
	CGameModel_LoadName(&model, sys->name);
	cDebrisSys_FreeActive(sys);
	cDebrisSys_FreeIdle(sys);
	cDebrisSys_dtor(sys);
	operator_delete(sys);
	CGameModel_Release(&model, 1);
	CGameModel_Reset(&model);
	return 1;
}

/* the weapon ring (iNext, iPrev) in the order of the engine variable PriorityWeapon, its default if that one is bad */
void cWorld_FillWeapPriority(cWorld *world) {
	char buf[0x400];
	uint32_t order[128];
	char seen[128];
	char *p = buf, *comma;
	int n = 0, count, len, cur;

	Str_Copy(buf, *g_PriorityWeapon, sizeof(buf));
	memset(order, 0, sizeof(order));
	memset(seen, 0, sizeof(seen));
	for (comma = strchr(p, ','); comma; comma = strchr(p, ',')) {
		*comma = 0;
		Str_TrimBlanks(p);
		order[n] = 0;
		for (int i = 0; i < world->weapons.count; i++) {
			if (!crt_stricmp(p, TABLE_AT(world->weapons, i)->name)) {
				order[n] = i;
				break;
			}
		}
		n++;
		p = comma + 1;
	}
	Str_TrimBlanks(p);
	for (int i = 0; i < world->weapons.count; i++) {
		if (!strcmp(p, TABLE_AT(world->weapons, i)->name)) {
			order[n] = i;
			break;
		}
	}
	for (int i = 0; i < world->weapons.count;) {
		uint32_t a = order[i];
		i++;
		uint32_t b = order[i % world->weapons.count];
		TABLE_AT(world->weapons, a)->next = b;
		TABLE_AT(world->weapons, b)->prev = a;
	}
	world->firstWeapon = order[0];
	len = 0;
	cur = 0;
	if (!seen[0]) {
		do {
			seen[cur] = 1;
			cur = TABLE_AT(world->weapons, cur)->next;
			len++;
		} while (!seen[cur]);
	}
	count = world->weapons.count;
	if (len != count) {
		if (strcmp(g_PriorityWeapon[0], g_PriorityWeapon[1])) {
			Con_Warnf("INVALID WEAPON ORDER:\n PriorityWeapons = %s\n using default", g_PriorityWeapon[0]);
			cString_Assign(&g_PriorityWeapon[0], &g_PriorityWeapon[1]);
			cWorld_FillWeapPriority(world);
			return;
		}
		Con_Bluef("INVALID DEFAULT WEAPON ORDER:\n PriorityWeapons = %s\n", g_PriorityWeapon[0]);
		CHECK(0);
	}
}

const char *cWorld_GetCamName(cWorld *world, int i) {
	return world->camName[i];
}

cEntityList *cWorld_GetList(cWorld *world, const char *name) {
	cPtrItem *it = PtrTable_Find(&world->lists, name, HashTable_Slot(&world->lists, name));

	return it ? it->value : NULL;
}

cEntity *cWorld_GetFirstEntity(cWorld *world) {
	Nto1Node *n = world->entityList.next;

	if (n->data && n) {
		return n->data;
	}
	return NULL;
}

float cWorld_GetMinCamDistSq(cWorld *world, const vec3_t *pos) {
	float best = FLT_MAX_F;

	for (int i = 0; i < 4; i++) {
		if ((uintptr_t)world->camName[i]) {
			const vec3_t *c = &world->camPos[i];
			float dx = c->x - pos->x;
			float dy = c->y - pos->y;
			float dz = c->z - pos->z;
			best = Float_MinSS(dy * dy + dx * dx + dz * dz, best);
		}
	}
	/* the instance has no cameras: its players are the viewers */
	return Players_Net() ? Float_MinSS(Players_MinDistSq(pos), best) : best;
}

/* the data of the "Obj" entity name of g_pWorld (self is not used) */
void *cWorld_GetObjData(cWorld *self, const char *name) {
	(void)self;
	if (*name) {
		cEntity *e = World_Lookup(g_pWorld, name);
		if (e && Entity_IsType(e, "Obj")) {
			return Obj_GetModel(e);
		}
	}
	return NULL;
}

cDebrisSys *cWorld_GetDebrisSys(cWorld *world, const char *name) {
	cPtrItem *it = PtrTable_Find(&world->debris, name, HashTable_Slot(&world->debris, name));

	if (it) {
		return it->value;
	}
	void *mem = operator_new(sizeof(cDebrisSys));
	cDebrisSys *sys = mem ? cDebrisSys_ctor(mem) : NULL;
	cDebrisSys_SetName(sys, name);
	sys->next = world->debrisList;
	world->debrisList = sys;
	PtrTable_Add(&world->debris, sys->name, (void *const *)&sys, 0);
	return sys;
}

void cWorld_ResetLogic(cWorld *self) {
	Logic_Reset(&self->logic);
}

byte World_IsEasySingle(void) {
	return g_GameSkill == 0 && g_pApp->flags == 0 && g_SplitScreenGame == 0;
}

byte World_IsHardSingle(void) {
	return g_GameSkill == 2 && g_pApp->flags == 0 && g_SplitScreenGame == 0;
}

/* the nearest hit of the segment on the entities of mask (not exclude, not ignored); the ignore list is emptied */
byte cWorld_TestLineEntities(cWorld *world, cEntity *exclude, uint32_t mask, const vec3_t *from, const vec3_t *to,
	vec3_t *hit, cEntity **target, int flag) {
	byte found = 0;
	vec3_t ext;

	world->testing = (byte)flag;
	hit->x = to->x;
	hit->y = to->y;
	hit->z = to->z;
	if (target) {
		*target = NULL;
	}
	CHECK(Grid_IsOk(&world->grid));
	ext.x = ext.y = ext.z = g_MaxRadObj * 2.0f;
	for (cEntity *e = Grid_FirstOnSegment(&world->grid, from, to, &ext); e; e = Grid_NextOnSegment(&world->grid)) {
		if (e == exclude || !(Entity_Mask(e) & mask) || World_IsIgnoredInline(world, e)) {
			continue;
		}
		if (Entity_TestLine(e, from, hit)) {
			if (target) {
				*target = e;
			}
			found = 1;
		}
	}
	memset(world->ignore, 0, sizeof(world->ignore));
	return found;
}

void *cWorld_NewEntityList(cWorld *world, const char *name) {
	void *mem = operator_new(sizeof(cEntityList));
	void *list = mem ? cEntityList_ctor(mem, name) : NULL;

	PtrTable_Add(&world->lists, *(char **)list, (void *const *)&list, 1);
	return list;
}

/* the debug text of the debris systems (O_Show) from screen position x, y */
void cWorld_ShowDebrisInfo(cWorld *self, int x, int y) {
	float line = (float)y;

	for (cDebrisSys *d = self->debrisList; d; d = d->next) {
		cDebrisSys_ShowInfo(d, x, (int)line, &line);
	}
}

/* view i shows the camera entity name ("" none); 2 if there is no such camera */
int cWorld_SetCam(cWorld *world, int i, const char *name) {
	cEntity *cam = NULL;

	CHECK(name != NULL);
	if (*name) {
		cPtrItem *it = PtrTable_Find(&world->entities, name, HashTable_Slot(&world->entities, name));
		if (it) {
			cam = it->value;
		}
		if (!cam || !Entity_IsType(cam, "Cam")) {
			return 2;
		}
	}
	char *key = world->camName[i];
	if (*key) {
		cEntity *old = World_Lookup(world, key);
		if (old && Entity_IsType(old, "Cam")) {
			CamEntity_Slot(old) = -1;
		}
	}
	if (cam) {
		if (!g_SplitScreenGame) {
			Camera_SetCurrent((CGameCamera *)CamEntity_Cam(cam));
		}
		world->camPos[i] = *CamEntity_Cam(cam);
		CamEntity_Slot(cam) = i;
		Str_Copy(key, name, 0x80);
		return 0;
	}
	if (!g_SplitScreenGame) {
		Camera_SetCurrent(NULL);
	}
	*key = 0;
	return 0;
}

/* the sound listener of each view at its camera; the velocity from the last position, or of the followed entity */
static void cWorld_UpdateListeners(cWorld *self, float dt) {
	for (int i = 0; i < 4; i++) {
		char *key = self->camName[i];
		vec3_t *last = &self->camPos[i];
		if (!*key) {
			continue;
		}
		cEntity *cam = World_Lookup(self, key);
		if (!cam || !Entity_IsType(cam, "Cam")) {
			continue;
		}
		if (Entity_IsType(cam, "FCam") && cCamEntity_GetTarget(cam) && !((cCamControl *)cam)->link[0]) {
			cEntity *t = cCamEntity_GetTarget(cam);
			const CSoundSysVtbl *svt = g_pSound->vftable;
			vec3_t vel, pos;
			vec3_t *pv = Obj_GetVel(t, &vel);
			vec3_t *pp = Obj_GetViewPos(t, &pos);
			Snd_SetListener(svt)(g_pSound, i, pp, CamEntity_Orient(cam), pv);
		} else {
			float inv = 1.0f / dt;
			const vec3_t *p = CamEntity_Cam(cam);
			vec3_t v;
			v.x = inv * (p->x - last->x);
			v.y = (p->y - last->y) * inv;
			v.z = (p->z - last->z) * inv;
			Snd_SetListener(g_pSound->vftable)(g_pSound, i, p, CamEntity_Orient(cam), &v);
		}
		*last = *CamEntity_Cam(cam);
	}
}

/* one frame of the world: time, scheduled functions, entities, debris, deleted entities and the subsystems */
void cWorld_Update(cWorld *self, float dt) {
	cTimer *timer = &self->timer;
	Nto1Node *n;

	if (g_SplitScreenGame && g_SplitScreen) {
		if (cTimer_GetSpeed(timer) > 0.0f) {
			g_SplitScreenUpdate = 1;
		} else {
			float t = Timer_GetTime(timer);
			g_SplitScreenUpdate = 0;
			if (0.25f >= t) {
				g_SplitScreenUpdate = 1;
			}
		}
		int c = g_SplitScreenCamera;
		self->curCam = c;
		char *key = self->camName[c];
		if (*key) {
			cEntity *cam = World_Lookup(self, key);
			if (cam) {
				CHECK(Entity_IsType(cam, "Cam"));
				Camera_SetCurrent((CGameCamera *)CamEntity_Cam(cam));
			}
		}
	}
	if (cTimer_GetSpeed(timer) > 0.0f) {
		cTimer_Advance(timer, dt);
		Players_BindTurn();
		Scheduler_Run(&self->scheduler, Timer_GetTime(timer));
		/* not in the original: the test channel answers here while the clock runs, where the script function of the
		 * old test harness ran (a scheduled function) */
		TestChannel_WorldFrame();
		for (n = self->entityList.next; n->data && n; n = n->next) {
			cEntity *e = n->data;
			/* the instance: the state of the player nearest to it while it updates (include/scrap/players.h) */
			if (g_Instance) {
				Players_BindNearest(&e->pos);
			}
			if (e->active) {
				Entity_Update(e);
			} else {
				Entity_UpdateOff(e);
			}
		}
		for (cDebrisSys *d = self->debrisList; d; d = d->next) {
			cDebrisSys_Update(d, cTimer_GetDelta(timer));
		}
		Grid_EndFrame(&self->grid);
		for (;;) {
			n = self->deadList.next;
			if (!n->data || !n) {
				break;
			}
			cEntity *e = n->data;
			PtrTable_Delete(&self->entities, e->name);
			Entity_Free(e);
		}
		Players_Clear();
		for (n = self->entityList.next; n->data && n; n = n->next) {
			cEntity *e = n->data;
			uint32_t slot;
			if (!e->active || !Entity_IsType(e, TYEN_WITH_LIFE)) {
				continue;
			}
			if (Entity_IsType(e, TYEN_CAR)) {
				slot = cVehicleEntity_GetOwnerSlot(e);
			} else if (Entity_IsType(e, TYEN_WALKCHAR)) {
				slot = cWalkCharEntity_GetOwnerSlot(e);
			} else {
				continue;
			}
			Players_SetOwner((int)slot, e);
		}
		Sound_Update(cTimer_GetDelta(timer));
		cWorld_UpdateListeners(self, cTimer_GetDelta(timer));
		Players_UpdateAlarms(cTimer_GetDelta(timer));
		cStrMoney_Update(self->strMoney, cTimer_GetDelta(timer));
		if (g_VehicleGroup) {
			AIGroup_Update(g_VehicleGroup, cTimer_GetDelta(timer));
		}
		if (g_CharGroup) {
			AIGroup_Update(g_CharGroup, cTimer_GetDelta(timer));
		}
		LogicControl_Update(&self->logic, cTimer_GetDelta(timer));
		Elements_Update(&self->elements, cTimer_GetDelta(timer));
	} else {
		Sound_UpdatePaused();
	}
	if (self->checkInput) {
		World_InputEvents();
	}
}

/* calls fn(e, ctx) for every entity of mask in the sphere, seen from its center unless noWall */
byte cWorld_SphereEach(cWorld *self, uint32_t mask, const vec3_t *center, float radius, int noWall,
	void (*fn)(cEntity *e, void *ctx), void *ctx) {
	byte found = 0;
	vec3_t lo, hi, pos;

	self->testing = 1;
	CHECK(Grid_IsOk(&self->grid));
	World_SphereBox(center, radius, &lo, &hi);
	for (cEntity *e = Grid_FirstInBox(&self->grid, &lo, &hi); e; e = Grid_NextInBox(&self->grid)) {
		if (!(Entity_Mask(e) & mask) || !Entity_TestSphere(e, NULL, center, radius, 0)) {
			continue;
		}
		if (!(byte)noWall && Engine_TestLine(center, Entity_GetPos(e, &pos), NULL, NULL)) {
			continue;
		}
		fn(e, ctx);
		found = 1;
	}
	return found;
}

static void World_SphereFunc(cEntity *e, void *func) {
	cPy_Function_AddParamS(func, e->name);
	cPy_Function_Execute(func, 0);
}

/* calls func(name) for every entity of mask in the sphere, seen from its center unless noWall */
byte cWorld_SphereCallF(cWorld *self, uint32_t mask, const vec3_t *center, float radius,
	cPy_Function *func, int noWall) {
	return cWorld_SphereEach(self, mask, center, radius, noWall, World_SphereFunc, func);
}

byte cWorld_SphereCall(cWorld *world, uint32_t mask, const vec3_t *center, float radius, const char *func,
	int flag) {
	cPy_Function f;

	cPy_Function_ctor(&f);
	cPy_Function_SetFunc(&f, func);
	byte r = cWorld_SphereCallF(world, mask, center, radius, &f, flag);
	cPy_Function_dtor(&f);
	return r;
}

/* the entities of mask (not exclude, not ignored) in the sphere, the last one in *target; the ignore list is emptied */
byte cWorld_TestSphereEntities(cWorld *world, cEntity *exclude, uint32_t mask, const vec3_t *center,
	float radius, cEntity **target, int a, const vec3_t *prev) {
	byte found = 0;
	vec3_t lo, hi;

	world->testing = (byte)a;
	if (target) {
		*target = NULL;
	}
	CHECK(Grid_IsOk(&world->grid));
	World_SphereBox(center, radius, &lo, &hi);
	for (cEntity *e = Grid_FirstInBox(&world->grid, &lo, &hi); e; e = Grid_NextInBox(&world->grid)) {
		if (!(Entity_Mask(e) & mask) || e == exclude || World_IsIgnoredInline(world, e)) {
			continue;
		}
		if (Entity_TestSphere(e, exclude, center, radius, prev)) {
			if (target) {
				*target = e;
			}
			found = 1;
		}
	}
	memset(world->ignore, 0, sizeof(world->ignore));
	return found;
}

/* the vtable of cWorld */

cGVar GVar_R_SplitScreenUpdate;
static cGVar GVar_PriorityWeapon;
static cGVarFunction GVar_QChatEvent2;
static cGVarFunction GVar_EscapeEvent;
static cGVarFunction GVar_QChatEvent3;
static cGVarFunction GVar_MainMissionEvent;
static cGVarFunction GVar_QChatEvent6;
static cGVar GVar_GridScaleY;
static cGVarFunction GVar_QChatEvent5;
static cGVar GVar_GridScaleZ;
static cGVar GVar_GameSkill;
static cGVar GVar_GridScaleX;
static cGVarFunction GVar_QChatEvent1;
/* engine variables (Gvars) */
static cGVar GVar_Max_Rad_Obj;
static cGVarFunction GVar_QChatEvent4;
static cGVarFunction GVar_ChatEvent;
cGVar GVar_FreeSpaceMagnet;
cGVar GVar_FXVehicleFloatingFactor;

/* the dynamic initializers of the globals here, run in the order of src/standalone/crtxc.inc */
INIT_GVAR_FLOAT(Init_GVar_FXVehicleFloatingFactor, &GVar_FXVehicleFloatingFactor, "FXVehicleFloatingFactor", 0, "Factor of floating Movement of the ship.", 1.0f)
INIT_GVAR_INT(Init_GVar_FreeSpaceMagnet, &GVar_FreeSpaceMagnet, "FreeSpaceMagnet", 0, "1 to activate the freespace sphere", 0)
FINI_GVAR_STRING(Fini_GVar_PriorityWeapon, &GVar_PriorityWeapon)
INIT_GVAR_STRING(Init_GVar_PriorityWeapon, Fini_GVar_PriorityWeapon, &GVar_PriorityWeapon, "PriorityWeapon", 2, "The Seventh Weapon\n", "Laser, Vulcan, Devastator, Tesla, ATPC, Swarm, Inferno")
FINI_CALL(Fini_GVar_EscapeEvent, &GVar_EscapeEvent.func, cPy_Function_dtor)
INIT_GVAR_FUNCTION(Init_GVar_EscapeEvent, Fini_GVar_EscapeEvent, &GVar_EscapeEvent, "EscapeEvent", 0, "EscapeEvent(id) <MainMenu> event when input is not in ´menu´ mode.", NULL)
FINI_CALL(Fini_GVar_MainMissionEvent, &GVar_MainMissionEvent.func, cPy_Function_dtor)
INIT_GVAR_FUNCTION(Init_GVar_MainMissionEvent, Fini_GVar_MainMissionEvent, &GVar_MainMissionEvent, "MainMissionEvent", 0, "MainMissionEvent(id) <Mission menu> event when input is not in ´menu´ mode.", NULL)
FINI_CALL(Fini_GVar_QChatEvent6, &GVar_QChatEvent6.func, cPy_Function_dtor)
INIT_GVAR_FUNCTION(Init_GVar_QChatEvent6, Fini_GVar_QChatEvent6, &GVar_QChatEvent6, "QChatEvent6", 0, "QChatEvent6(id) only for racer input modes. The quick chat message has been pressed.\n", NULL)
FINI_CALL(Fini_GVar_QChatEvent5, &GVar_QChatEvent5.func, cPy_Function_dtor)
INIT_GVAR_FUNCTION(Init_GVar_QChatEvent5, Fini_GVar_QChatEvent5, &GVar_QChatEvent5, "QChatEvent5", 0, "QChatEvent5(id) only for racer input modes. The quick chat message has been pressed.", NULL)
FINI_CALL(Fini_GVar_QChatEvent4, &GVar_QChatEvent4.func, cPy_Function_dtor)
INIT_GVAR_FUNCTION(Init_GVar_QChatEvent4, Fini_GVar_QChatEvent4, &GVar_QChatEvent4, "QChatEvent4", 0, "QChatEvent4(id) only for racer input modes. The quick chat message has been pressed.", NULL)
FINI_CALL(Fini_GVar_QChatEvent3, &GVar_QChatEvent3.func, cPy_Function_dtor)
INIT_GVAR_FUNCTION(Init_GVar_QChatEvent3, Fini_GVar_QChatEvent3, &GVar_QChatEvent3, "QChatEvent3", 0, "QChatEvent3(id) only for racer input modes. The quick chat message has been pressed.", NULL)
FINI_CALL(Fini_GVar_QChatEvent2, &GVar_QChatEvent2.func, cPy_Function_dtor)
INIT_GVAR_FUNCTION(Init_GVar_QChatEvent2, Fini_GVar_QChatEvent2, &GVar_QChatEvent2, "QChatEvent2", 0, "QChatEvent2(id) only for racer input modes. The quick chat message has been pressed.", NULL)
FINI_CALL(Fini_GVar_QChatEvent1, &GVar_QChatEvent1.func, cPy_Function_dtor)
INIT_GVAR_FUNCTION(Init_GVar_QChatEvent1, Fini_GVar_QChatEvent1, &GVar_QChatEvent1, "QChatEvent1", 0, "QChatEvent1(id) only for racer input modes. The quick chat message has been pressed.", NULL)
FINI_CALL(Fini_GVar_ChatEvent, &GVar_ChatEvent.func, cPy_Function_dtor)
INIT_GVAR_FUNCTION(Init_GVar_ChatEvent, Fini_GVar_ChatEvent, &GVar_ChatEvent, "ChatEvent", 0, "ChatEvent(id) only for racer input modes.   The chat key has been pressed.", NULL)
INIT_GVAR_INT(Init_GVar_GameSkill, &GVar_GameSkill, "GameSkill", 0, "Game Skill. (0 = Easy,1 = Normal,2 = Hard).\n", 1)
INIT_GVAR_FLOAT(Init_GVar_GridScaleZ, &GVar_GridScaleZ, "GridScaleZ", 0, "Z Scale of the entity grid cell\n", 20000.0f)
INIT_GVAR_FLOAT(Init_GVar_GridScaleX, &GVar_GridScaleX, "GridScaleX", 0, "X Scale of the entity grid cell", 20000.0f)
INIT_GVAR_FLOAT(Init_GVar_GridScaleY, &GVar_GridScaleY, "GridScaleY", 0, "Y Scale of the entity grid cell", 20000.0f)
INIT_GVAR_FLOAT(Init_GVar_Max_Rad_Obj, &GVar_Max_Rad_Obj, "Max_Rad_Obj", 0, "Min radius of the obj scale\n", 10000.0f)
INIT_GVAR_INT(Init_GVar_R_SplitScreenUpdate, &GVar_R_SplitScreenUpdate, "R_SplitScreenUpdate", 0, "Split Screen Update Target", 0)
