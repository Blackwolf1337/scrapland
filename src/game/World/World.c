/*
 * World/World.cpp of the game: cWorld, the world of the game entities, see include/scrap/world.h.
 */
#include <string.h>

#include "scrap/ai.h"
#include "scrap/console.h"
#include "scrap/crt.h"
#include "scrap/hashtable.h"
#include "scrap/input.h"
#include "scrap/material.h"
#include "scrap/scheduler.h"
#include "scrap/scrapcollision.h"
#include "scrap/scrapmain.h"
#include "scrap/scrapmodel.h"
#include "scrap/scrapworld.h"
#include "scrap/sound.h"
#include "scrap/world.h"

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

typedef cEntity *(__thiscall *AIGetTarget_t)(void *ai);
typedef void(__thiscall *AIGroupUpdate_t)(void *group, float dt);

/* the target of an AI (vtable slot 0x48) */
static cEntity *AI_GetTarget(void *ai) {
	return (*(AIGetTarget_t *)(*(byte **)ai + 0x48))(ai);
}

/* the update of an AI group (vtable slot 0x20) */
static void AIGroup_Update(void *group, float dt) {
	(*(AIGroupUpdate_t *)(*(byte **)group + 0x20))(group, dt);
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

/* v against the view direction of the angles (x pitch, y yaw): the dot product of the facing axis */
float Vec3_DotViewDir(const vec3_t *v, float ax, float ay) {
	float sx, cx, sy, cy;

	X87_SinCos(-ax, &sx, &cx);
	X87_SinCos(ay + PI_F, &sy, &cy);
	return (cy * v->z + sy * v->x) * cx - sx * v->y;
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

	WEnt_Vtbl(self)->GetView(self, &pos, &ang);
	ang.x = ang.x + PI_F;
	byte hasAI = WEnt_Vtbl(self)->GetAI(self) != NULL;
	cEntity *e = hasAI ? AI_GetTarget(WEnt_Vtbl(self)->GetAI(self)) : cWorld_GetFirstEntity(g_pWorld);
	if (!e) {
		return NULL;
	}
	for (;;) {
		if (e->flag_4c && e != self && WEnt_Vtbl(e)->IsType(e, TYEN_CAR)) {
			int take;
			if (hasAI || mode == 2) {
				take = 1;
			} else if (mode == 1) {
				int et = Entity_Team(e), st = Entity_Team(self);
				take = et != 3 && st != 3 && (et != st || et == 4 || st == 4);
			} else {
				take = mode == 0 && Entity_Team(e) != 3;
			}
			if (take) {
				vec3_t tp = *WEnt_Vtbl(e)->GetPos(e, &tmp);
				float dx = tp.x - pos.x;
				float dy = tp.y - pos.y;
				float dz = tp.z - pos.z;
				float d2 = dy * dy + dx * dx + dz * dz;
				if (range2 > d2) {
					float dist = (float)__builtin_sqrt((double)d2);
					float inv = 1.0f / dist;
					float sx, cx, sy, cy;
					float nx = dx * inv, ny = dy * inv, nz = dz * inv;
					X87_SinCos(-ang.x, &sx, &cx);
					X87_SinCos(ang.y + PI_F, &sy, &cy);
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

cAmmoInfo *__thiscall cAmmoInfo_ctor(cAmmoInfo *self) {
	cString_ctor(&self->name);
	cPy_Function_ctor(&self->onPress);
	cPy_Function_ctor(&self->onRelease);
	cPy_Function_ctor(&self->onFire);
	cPy_Function_ctor(&self->onHit);
	self->max = 0.0f;
	self->charger = 0.0f;
	return self;
}

cWeapInfo *__thiscall cWeapInfo_ctor(cWeapInfo *self) {
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

void __fastcall cAmmoInfo_dtor(cAmmoInfo *self) {
	cPy_Function_dtor(&self->onHit);
	cPy_Function_dtor(&self->onFire);
	cPy_Function_dtor(&self->onRelease);
	cPy_Function_dtor(&self->onPress);
	cString_dtor(&self->name);
}

void __fastcall cWeapInfo_dtor(cWeapInfo *self) {
	cPy_Function_dtor(&self->setFx);
	cPy_Function_dtor(&self->onEnd);
	cPy_Function_dtor(&self->onStart);
	cPy_Function_dtor(&self->onHit);
	cPy_Function_dtor(&self->onFire);
	cPy_Function_dtor(&self->onRelease);
	cPy_Function_dtor(&self->onPress);
	cString_dtor(&self->name);
}

/* the vector deleting destructors */
cAmmoInfo *__thiscall cAmmoInfo_Delete(cAmmoInfo *self, uint32_t flags) {
	if (flags & 2) {
		return Delete_VectorPart(self, flags, sizeof(*self), CAMMOINFO_DTOR);
	}
	cAmmoInfo_dtor(self);
	if (flags & 1) {
		operator_delete_sized(self, sizeof(*self));
	}
	return self;
}

cWeapInfo *__thiscall cWeapInfo_Delete(cWeapInfo *self, uint32_t flags) {
	if (flags & 2) {
		return Delete_VectorPart(self, flags, sizeof(*self), CWEAPINFO_DTOR);
	}
	cWeapInfo_dtor(self);
	if (flags & 1) {
		operator_delete_sized(self, sizeof(*self));
	}
	return self;
}

/* the Global/Table.h Init of the two instances: new T[num] with the constructors */
void __thiscall AmmoTable_Init(void *table, int num) {
	TABLE(cAmmoInfo) *t = table;

	CHECK(!t->items && num > 0);
	t->count = num;
	t->items = New_Objects(num, sizeof(cAmmoInfo), CAMMOINFO_CTOR, CAMMOINFO_DTOR);
	CHECK(t->items);
}

void __thiscall WeapTable_Init(void *table, int num) {
	TABLE(cWeapInfo) *t = table;

	CHECK(!t->items && num > 0);
	t->count = num;
	t->items = New_Objects(num, sizeof(cWeapInfo), CWEAPINFO_CTOR, CWEAPINFO_DTOR);
	CHECK(t->items);
}

int __thiscall cAmmoInfo_GetF(cAmmoInfo *ammo, const char *var, float *value) {
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

int __thiscall cWeapInfo_GetF(cWeapInfo *weap, const char *var, float *value) {
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

int __thiscall cAmmoInfo_GetS(cAmmoInfo *ammo, const char *var, char **value) {
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

int __thiscall cWeapInfo_GetS(cWeapInfo *weap, const char *var, char **value) {
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

int __thiscall cAmmoInfo_SetF(cAmmoInfo *ammo, const char *var, float value) {
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

int __thiscall cWeapInfo_SetF(cWeapInfo *weap, const char *var, float value) {
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

int __thiscall cAmmoInfo_SetS(cAmmoInfo *ammo, const char *var, const char *value) {
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

int __thiscall cWeapInfo_SetS(cWeapInfo *weap, const char *var, const char *value) {
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

cWorld *__thiscall cWorld_ctor(cWorld *self) {
	cScheduledFuncStack *sched = (cScheduledFuncStack *)self->scheduler;
	box_t box;
	vec3_t cell;

	self->vftable = Game_Ptr(CWORLD_VFTABLE);
	HashTable_ctor(&self->entities, 0x1c9, 1);
	Nto1Node_Init(&self->entityList);
	Nto1Node_Init(&self->deadList);
	memset(&sched->list, 0, sizeof(sched->list));
	sched->vftable = Game_Ptr(CSCHEDULEDFUNCSTACK_VFTABLE);
	HashTable_ctor(&self->debris, 0x1c9, 1);
	HashTable_ctor(&self->lists, 0x1f, 1);
	cTimer_ctor(&self->timer);
	Floor_ctor(self->floor);
	Elements_ctor(self->elements);
	self->weapons.items = NULL;
	self->weapons.count = 0;
	self->ammo.items = NULL;
	self->ammo.count = 0;
	Grid_ctor(self->grid);
	Alarm_ctor(self->alarm);
	cStrMoney_ctor(self->strMoney);
	Logic_ctor(self->taunts);
	self->nameCount = 0;
	self->testing = 0;
	self->entityList.target = self;
	self->deadList.target = self;
	World_GetCollideBox(&box);
	cell.x = g_GridScaleX;
	cell.y = g_GridScaleY;
	cell.z = g_GridScaleZ;
	Grid_Init(self->grid, &box.min, &box.max, &cell);
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

void __fastcall AmmoTable_Free(void *table) {
	cAmmoTable *t = table;

	if (t->items) {
		Delete_Objects(t->items, sizeof(cAmmoInfo), CAMMOINFO_DTOR);
		t->items = NULL;
		t->count = 0;
	}
}

void __fastcall WeapTable_Free(void *table) {
	cWeapTable *t = table;

	if (t->items) {
		Delete_Objects(t->items, sizeof(cWeapInfo), CWEAPINFO_DTOR);
		t->items = NULL;
		t->count = 0;
	}
}

void __fastcall cWorld_dtor(cWorld *self) {
	self->vftable = Game_Ptr(CWORLD_VFTABLE);
	cWorld_Clear(self);
	Logic_dtor(self->taunts);
	Grid_dtor(self->grid);
	AmmoTable_Free(&self->ammo);
	WeapTable_Free(&self->weapons);
	Elements_dtor(self->elements);
	Floor_dtor(self->floor);
	PtrTable_Clear(&self->lists);
	PtrTable_Clear(&self->debris);
	cScheduledFuncStack_dtor(self->scheduler);
	Nto1Node_Done(&self->deadList);
	Nto1Node_Done(&self->entityList);
	PtrTable_Clear(&self->entities);
}

cWorld *__thiscall cWorld_Destroy(cWorld *self, byte flags) {
	cWorld_dtor(self);
	if (flags & 1) {
		operator_delete_sized(self, CWORLD_SIZE);
	}
	return self;
}

void __thiscall cWorld_AddIgnore(cWorld *self, cEntity *e) {
	for (int i = 0; i < 4; i++) {
		if (!self->ignore[i]) {
			self->ignore[i] = e;
			return;
		}
	}
	CHECK(0);
}

byte __thiscall cWorld_IsIgnored(cWorld *self, cEntity *e) {
	return World_IsIgnoredInline(self, e);
}

/* calls func(name, x, y, z, dist) for every entity of mask within radius of the segment, seen from it */
byte __thiscall cWorld_LineCallF(cWorld *self, uint32_t mask, const vec3_t *from, const vec3_t *to, float radius,
	cPy_Function *func) {
	byte found = 0;
	vec3_t ext, closest, pos, pos2;

	self->testing = 1;
	CHECK(Grid_IsOk(self->grid));
	ext.x = ext.y = ext.z = g_MaxRadObj + radius;
	for (cEntity *e = Grid_FirstOnSegment(self->grid, from, to, &ext); e; e = Grid_NextOnSegment(self->grid)) {
		if (!(Entity_Mask(e) & mask)) {
			continue;
		}
		float d2 = Dist2_PointSegment(from, to, WEnt_Vtbl(e)->GetPos(e, &pos), &closest);
		if (!WEnt_Vtbl(e)->TestSphere(e, NULL, &closest, radius, 0)) {
			continue;
		}
		if (Engine_TestLine(&closest, WEnt_Vtbl(e)->GetPos(e, &pos2), NULL, NULL)) {
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

byte __thiscall cWorld_LineCall(cWorld *self, uint32_t mask, const vec3_t *from, const vec3_t *to, float radius,
	const char *func) {
	cPy_Function f;

	cPy_Function_ctor(&f);
	cPy_Function_SetFunc(&f, func);
	byte r = cWorld_LineCallF(self, mask, from, to, radius, &f);
	cPy_Function_dtor(&f);
	return r;
}

/* the first entity of mask (not exclude) within radius of the segment */
byte __thiscall cWorld_LineFirst(cWorld *self, cEntity *exclude, uint32_t mask, const vec3_t *from, const vec3_t *to,
	float radius, cEntity **target) {
	vec3_t ext, closest, pos;

	if (target) {
		*target = NULL;
	}
	self->testing = 1;
	CHECK(Grid_IsOk(self->grid));
	ext.x = ext.y = ext.z = g_MaxRadObj + radius;
	for (cEntity *e = Grid_FirstOnSegment(self->grid, from, to, &ext); e; e = Grid_NextOnSegment(self->grid)) {
		if (!(Entity_Mask(e) & mask)) {
			continue;
		}
		Dist2_PointSegment(from, to, WEnt_Vtbl(e)->GetPos(e, &pos), &closest);
		if (e != exclude && WEnt_Vtbl(e)->TestSphere(e, NULL, &closest, radius, 0)) {
			if (target) {
				*target = e;
			}
			return 1;
		}
	}
	return 0;
}

/* action of an action set pressed in this frame, with the index checks of Table<T>::operator[] */
static byte Input_ActionPressed(uint32_t set, uint32_t action, int player) {
	cActionSet *s = TABLE_AT(g_ActionSets, set);

	return cInputAction_Pressed(TABLE_AT(s->actions, action), player);
}

/* the script events of the menu and chat keys of each player */
void World_InputEvents(void) {
	for (int v = 0; v < 4; v++) {
		if (v == 0) {
			if ((Input_GetActionSet() == 1 && Input_ActionPressed(1, 0x27, 0)) ||
				(Input_GetActionSet() == 3 && Input_ActionPressed(3, 0x17, 0))) {
				if (Talk_IsShown()) {
					g_RemainTalkTime = 0.0f;
				} else {
					cPy_Function_AddParamI(g_MainMissionEvent, 0);
					cPy_Function_Execute(g_MainMissionEvent, 0);
				}
			}
		}
		if ((byte)Talk_IsShown() && ((Input_GetActionSet() == 1 && Input_ActionPressed(1, 0, v)) ||
			(Input_GetActionSet() == 3 && Input_ActionPressed(3, 0, v)))) {
			g_RemainTalkTime = 0.0f;
		} else if (v == 0) {
			if ((Input_GetActionSet() == 1 && Input_ActionPressed(1, 0, 0)) ||
				(Input_GetActionSet() == 3 && Input_ActionPressed(3, 0, 0)) ||
				(Input_GetActionSet() == 2 && Input_ActionPressed(2, 0, 0))) {
				cPy_Function_AddParamI(g_EscapeEvent, 0);
				cPy_Function_Execute(g_EscapeEvent, 0);
			}
		}
		if (Input_GetActionSet() == 1) {
			cPy_Function *events[7] = { g_ChatEvent, g_QChatEvent1, g_QChatEvent2, g_QChatEvent3, g_QChatEvent4,
				g_QChatEvent5, g_QChatEvent6 };
			for (int k = 0; k < 7; k++) {
				if (Input_ActionPressed(1, 0x20 + k, v)) {
					cPy_Function_AddParamI(events[k], v);
					cPy_Function_Execute(events[k], 0);
					break;
				}
			}
		}
	}
}

cEntity *__thiscall cWorld_CreateEntity(cWorld *world, const char *className, float x, float y, float z,
	const char *type) {
	void *mem;
	cEntity *e;

	(void)world;
	if (!crt_stricmp(type, "Entity")) {
		mem = operator_new(0x7c);
		return mem ? cEntity_ctor(mem, className, x, y, z) : NULL;
	}
	if (!crt_stricmp(type, "CamPath")) {
		mem = operator_new(0x1758);
		return mem ? cCamPathEntity_ctor(mem, className, x, y, z) : NULL;
	}
	if (!crt_stricmp(type, TYEN_WALKCHAR)) {
		mem = operator_new(0x574);
		e = mem ? cWalkCharEntity_ctor(mem, className, x, y, z) : NULL;
		Grid_Add(g_pWorld->grid, e);
		return e;
	}
	if (!crt_stricmp(type, TYEN_CAR)) {
		mem = operator_new(0x814);
		e = mem ? cVehicleEntity_ctor(mem, className, x, y, z) : NULL;
		Grid_Add(g_pWorld->grid, e);
		return e;
	}
	if (!crt_stricmp(type, "Cam")) {
		mem = operator_new(0x13c);
		return mem ? cCamEntity_ctor(mem, className, x, y, z) : NULL;
	}
	if (!crt_stricmp(type, "FCam")) {
		mem = operator_new(0x244);
		return mem ? cFollowCamEntity_ctor(mem, className, x, y, z) : NULL;
	}
	if (!crt_stricmp(type, "Missile")) {
		mem = operator_new(0x7e8);
		return mem ? cMissileEntity_ctor(mem, className, x, y, z) : NULL;
	}
	if (!crt_stricmp(type, "item")) {
		mem = operator_new(0x398);
		return mem ? cItemEntity_ctor(mem, className, x, y, z) : NULL;
	}
	if (!crt_stricmp(type, TYEN_WITH_LIFE)) {
		mem = operator_new(1000);
		return mem ? cWithLifeEntity_ctor(mem, className, x, y, z) : NULL;
	}
	if (!crt_stricmp(type, "Obj")) {
		mem = operator_new(0x228);
		return mem ? cObjEntity_ctor(mem, className, x, y, z) : NULL;
	}
	return NULL;
}

/* the end of a level: scheduled functions, cameras, entities, debris systems and entity lists */
void __fastcall cWorld_Clear(cWorld *self) {
	Nto1Node *n;

	Scheduler_Clear((cScheduledFuncStack *)self->scheduler);
	for (n = self->entityList.next; n->data && n; n = n->next) {
		cEntity *e = n->data;
		if (WEnt_Vtbl(e)->IsType(e, "Cam")) {
			WEnt_Vtbl(e)->SetTarget(e, NULL);
		}
	}
	Camera_SetActual(NULL);
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
			WEnt_Vtbl(e)->Delete(e, 1);
		}
	}
	while (self->debrisList) {
		cDebrisSys *d = self->debrisList;
		self->debrisList = d->next;
		if (d) {
			cDebrisSys_dtor(d);
			operator_delete_sized(d, sizeof(*d));
		}
	}
	cWorld_DeleteLists(self);
	GameModelList_Free();
}

void __fastcall cWorld_DeleteLists(cWorld *self) {
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
		byte *list = it->value;
		CHECK(PtrTable_Delete(t, it->key));
		if (list) {
			/* name (string object), two functions, the entities */
			EntityDList_dtor(list + 0x30);
			cPy_Function_dtor((cPy_Function *)(list + 0x18));
			cPy_Function_dtor((cPy_Function *)(list + 0x4));
			cString_dtor((cString *)list);
			operator_delete_sized(list, 0x44);
		}
	}
}

byte __thiscall cWorld_DeleteDebrisSys(cWorld *world, const char *name) {
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
	operator_delete_sized(sys, sizeof(*sys));
	CGameModel_Release(&model, 1);
	CGameModel_Reset(&model);
	return 1;
}

/* the weapon ring (iNext, iPrev) in the order of the engine variable PriorityWeapon, its default if that one is bad */
void __thiscall cWorld_FillWeapPriority(cWorld *world) {
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

const char *__thiscall cWorld_GetCamName(cWorld *world, int i) {
	/* the original tests the address of the name for NULL */
	uintptr_t p = (uintptr_t)world + 0x54 + ((uint32_t)i << 7);

	return p ? (const char *)p : "";
}

cEntityList *__thiscall cWorld_GetList(cWorld *world, const char *name) {
	cPtrItem *it = PtrTable_Find(&world->lists, name, HashTable_Slot(&world->lists, name));

	return it ? it->value : NULL;
}

cEntity *__thiscall cWorld_GetFirstEntity(cWorld *world) {
	Nto1Node *n = world->entityList.next;

	if (n->data && n) {
		return n->data;
	}
	return NULL;
}

/* the distance to the nearest camera position */
float __thiscall cWorld_GetMinCamDist(cWorld *self, const vec3_t *pos) {
	float best = FLT_MAX_F;

	for (int i = 0; i < 4; i++) {
		if ((uintptr_t)self->camName[i]) {
			const vec3_t *c = &self->camPos[i];
			float dx = c->x - pos->x;
			float dy = c->y - pos->y;
			float dz = c->z - pos->z;
			float d = (float)__builtin_sqrt((double)(dy * dy + dx * dx + dz * dz));
			best = Float_MinSS(d, best);
		}
	}
	return best;
}

float __thiscall cWorld_GetMinCamDistSq(cWorld *world, const vec3_t *pos) {
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
	return best;
}

/* the data of the "Obj" entity name of g_pWorld (self is not used) */
void *__thiscall cWorld_GetObjData(cWorld *self, const char *name) {
	(void)self;
	if (*name) {
		cEntity *e = World_Lookup(g_pWorld, name);
		if (e && WEnt_Vtbl(e)->IsType(e, "Obj")) {
			return WEnt_Vtbl(e)->GetObjData(e);
		}
	}
	return NULL;
}

/* name, or name with a number when an entity has it, in a static buffer */
char *__thiscall cWorld_UniqueName(cWorld *self, const char *name) {
	char *buf = g_UniqueName;

	Str_Copy(buf, name, 0x80);
	while (buf[0]) {
		cPtrItem *it = PtrTable_Find(&self->entities, buf, HashTable_Slot(&self->entities, buf));
		if (!it || !it->value) {
			break;
		}
		Str_PrintfQuiet(buf, 0x80, "%s%i", name, self->nameCount++);
	}
	return buf;
}

cDebrisSys *__thiscall cWorld_GetDebrisSys(cWorld *world, const char *name) {
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

void __fastcall cWorld_ResetLogic(cWorld *self) {
	Logic_Reset(self->taunts);
}

byte World_IsEasySingle(void) {
	return g_GameSkill == 0 && g_pApp->flags == 0 && g_SplitScreenGame == 0;
}

byte World_IsHardSingle(void) {
	return g_GameSkill == 2 && g_pApp->flags == 0 && g_SplitScreenGame == 0;
}

/* the nearest hit of the segment on the entities of mask (not exclude, not ignored); the ignore list is emptied */
byte __thiscall cWorld_TestLineEntities(cWorld *world, int arg, void *list, const vec3_t *from, const vec3_t *to,
	vec3_t *hit, cEntity **target, int flag) {
	cEntity *exclude = (cEntity *)arg;
	uint32_t mask = (uint32_t)list;
	byte found = 0;
	vec3_t ext;

	world->testing = (byte)flag;
	hit->x = to->x;
	hit->y = to->y;
	hit->z = to->z;
	if (target) {
		*target = NULL;
	}
	CHECK(Grid_IsOk(world->grid));
	ext.x = ext.y = ext.z = g_MaxRadObj * 2.0f;
	for (cEntity *e = Grid_FirstOnSegment(world->grid, from, to, &ext); e; e = Grid_NextOnSegment(world->grid)) {
		if (e == exclude || !(Entity_Mask(e) & mask) || World_IsIgnoredInline(world, e)) {
			continue;
		}
		if (WEnt_Vtbl(e)->TestLine(e, from, hit)) {
			if (target) {
				*target = e;
			}
			found = 1;
		}
	}
	memset(world->ignore, 0, sizeof(world->ignore));
	return found;
}

void *__thiscall cWorld_NewEntityList(cWorld *world, const char *name) {
	void *mem = operator_new(0x44);
	void *list = mem ? cEntityList_ctor(mem, name) : NULL;

	PtrTable_Add(&world->lists, *(char **)list, (void *const *)&list, 1);
	return list;
}

/* the debug text of the debris systems (O_Show) from screen position x, y */
void __thiscall cWorld_ShowDebrisInfo(cWorld *self, int x, int y) {
	float line = (float)y;

	for (cDebrisSys *d = self->debrisList; d; d = d->next) {
		cDebrisSys_ShowInfo(d, x, (int)line, &line);
	}
}

/* view i shows the camera entity name ("" none); 2 if there is no such camera */
int __thiscall cWorld_SetCam(cWorld *world, int i, const char *name) {
	cEntity *cam = NULL;

	CHECK(name != NULL);
	if (*name) {
		cPtrItem *it = PtrTable_Find(&world->entities, name, HashTable_Slot(&world->entities, name));
		if (it) {
			cam = it->value;
		}
		if (!cam || !WEnt_Vtbl(cam)->IsType(cam, "Cam")) {
			return 2;
		}
	}
	char *key = world->camName[i];
	if (*key) {
		cEntity *old = World_Lookup(world, key);
		if (old && WEnt_Vtbl(old)->IsType(old, "Cam")) {
			CamEntity_Slot(old) = -1;
		}
	}
	if (cam) {
		if (!g_SplitScreenGame) {
			Camera_SetActual((CGameCamera *)CamEntity_Cam(cam));
		}
		world->camPos[i] = *CamEntity_Cam(cam);
		CamEntity_Slot(cam) = i;
		Str_Copy(key, name, 0x80);
		return 0;
	}
	if (!g_SplitScreenGame) {
		Camera_SetActual(NULL);
	}
	*key = 0;
	return 0;
}

/* the sound listener of each view at its camera; the velocity from the last position, or of the followed entity */
void __thiscall cWorld_UpdateListeners(cWorld *self, float dt) {
	for (int i = 0; i < 4; i++) {
		char *key = self->camName[i];
		vec3_t *last = &self->camPos[i];
		if (!*key) {
			continue;
		}
		cEntity *cam = World_Lookup(self, key);
		if (!cam || !WEnt_Vtbl(cam)->IsType(cam, "Cam")) {
			continue;
		}
		if (WEnt_Vtbl(cam)->IsType(cam, "FCam") && cCamEntity_GetTarget(cam) && !CamEntity_NoFollow(cam)) {
			cEntity *t = cCamEntity_GetTarget(cam);
			const CSoundSysVtbl *svt = g_pSound->vftable;
			vec3_t vel, pos;
			vec3_t *pv = WEnt_Vtbl(t)->GetVel(t, &vel);
			vec3_t *pp = WEnt_Vtbl(t)->GetCenter(t, &pos);
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
void __thiscall cWorld_Update(cWorld *self, float dt) {
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
				CHECK(WEnt_Vtbl(cam)->IsType(cam, "Cam"));
				Camera_SetActual((CGameCamera *)CamEntity_Cam(cam));
			}
		}
	}
	if (cTimer_GetSpeed(timer) > 0.0f) {
		cTimer_Advance(timer, dt);
		Scheduler_Run((cScheduledFuncStack *)self->scheduler, Timer_GetTime(timer));
		for (n = self->entityList.next; n->data && n; n = n->next) {
			cEntity *e = n->data;
			if (e->flag_4c) {
				WEnt_Vtbl(e)->Update(e);
			} else {
				WEnt_Vtbl(e)->UpdateOff(e);
			}
		}
		for (cDebrisSys *d = self->debrisList; d; d = d->next) {
			cDebrisSys_Update(d, cTimer_GetDelta(timer));
		}
		Grid_EndFrame(self->grid);
		for (;;) {
			n = self->deadList.next;
			if (!n->data || !n) {
				break;
			}
			cEntity *e = n->data;
			PtrTable_Delete(&self->entities, e->name);
			WEnt_Vtbl(e)->Delete(e, 1);
		}
		for (int i = 0; i < 4; i++) {
			self->owners[i] = NULL;
		}
		for (n = self->entityList.next; n->data && n; n = n->next) {
			cEntity *e = n->data;
			uint32_t slot;
			if (!e->flag_4c || !WEnt_Vtbl(e)->IsType(e, TYEN_WITH_LIFE)) {
				continue;
			}
			if (WEnt_Vtbl(e)->IsType(e, TYEN_CAR)) {
				slot = cVehicleEntity_GetOwnerSlot(e);
			} else if (WEnt_Vtbl(e)->IsType(e, TYEN_WALKCHAR)) {
				slot = cWalkCharEntity_GetOwnerSlot(e);
			} else {
				continue;
			}
			if (slot < 4) {
				self->owners[slot] = e;
			}
		}
		Sound_Update(cTimer_GetDelta(timer));
		cWorld_UpdateListeners(self, cTimer_GetDelta(timer));
		Alarm_Update(self->alarm, cTimer_GetDelta(timer));
		cStrMoney_Update(self->strMoney, cTimer_GetDelta(timer));
		if (g_VehicleGroup) {
			AIGroup_Update(g_VehicleGroup, cTimer_GetDelta(timer));
		}
		if (g_CharGroup) {
			AIGroup_Update(g_CharGroup, cTimer_GetDelta(timer));
		}
		Logic_Update(self->taunts, cTimer_GetDelta(timer));
		Elements_Update(self->elements, cTimer_GetDelta(timer));
	} else {
		Sound_UpdatePaused();
	}
	if (self->checkInput) {
		World_InputEvents();
	}
}

/* calls func(name) for every entity of mask in the sphere, seen from its center unless noWall */
byte __thiscall cWorld_SphereCallF(cWorld *self, uint32_t mask, const vec3_t *center, float radius,
	cPy_Function *func, int noWall) {
	byte found = 0;
	vec3_t lo, hi, pos;

	self->testing = 1;
	CHECK(Grid_IsOk(self->grid));
	World_SphereBox(center, radius, &lo, &hi);
	for (cEntity *e = Grid_FirstInBox(self->grid, &lo, &hi); e; e = Grid_NextInBox(self->grid)) {
		if (!(Entity_Mask(e) & mask) || !WEnt_Vtbl(e)->TestSphere(e, NULL, center, radius, 0)) {
			continue;
		}
		if (!(byte)noWall && Engine_TestLine(center, WEnt_Vtbl(e)->GetPos(e, &pos), NULL, NULL)) {
			continue;
		}
		cPy_Function_AddParamS(func, e->name);
		cPy_Function_Execute(func, 0);
		found = 1;
	}
	return found;
}

byte __thiscall cWorld_SphereCall(cWorld *world, void *list, const vec3_t *center, float radius, const char *func,
	int flag) {
	cPy_Function f;

	cPy_Function_ctor(&f);
	cPy_Function_SetFunc(&f, func);
	byte r = cWorld_SphereCallF(world, (uint32_t)list, center, radius, &f, flag);
	cPy_Function_dtor(&f);
	return r;
}

/* the entities of mask (not exclude, not ignored) in the sphere, the last one in *target; the ignore list is emptied */
byte __thiscall cWorld_TestSphereEntities(cWorld *world, cEntity *exclude, void *list, const vec3_t *center,
	float radius, cEntity **target, int a, int b) {
	uint32_t mask = (uint32_t)list;
	byte found = 0;
	vec3_t lo, hi;

	world->testing = (byte)a;
	if (target) {
		*target = NULL;
	}
	CHECK(Grid_IsOk(world->grid));
	World_SphereBox(center, radius, &lo, &hi);
	for (cEntity *e = Grid_FirstInBox(world->grid, &lo, &hi); e; e = Grid_NextInBox(world->grid)) {
		if (!(Entity_Mask(e) & mask) || e == exclude || World_IsIgnoredInline(world, e)) {
			continue;
		}
		if (WEnt_Vtbl(e)->TestSphere(e, exclude, center, radius, b)) {
			if (target) {
				*target = e;
			}
			found = 1;
		}
	}
	memset(world->ignore, 0, sizeof(world->ignore));
	return found;
}
