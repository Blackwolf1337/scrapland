/*
 * The controls of a vehicle, see include/scrap/controls.h.
 */
#include <string.h>

#include "scrap/camcontrol.h"
#include "scrap/controls.h"
#include "scrap/input.h"
#include "scrap/itementity.h"
#include "scrap/mathlib.h"
#include "scrap/net.h"
#include "scrap/scoreritems.h"
#include "scrap/timer.h"
#include "scrap/vehicleentity.h"
#include "scrap/weapons.h"
#include "scrap/weaps2.h"
#include "scrap/withlifeentity.h"
#include "scrap/world.h"

#define CONTROLS_MIN_FADE 0.01f		/* rva 0x3b10d8 */

static inline cVehicleControls *Ctl(byte *p) {
	return (cVehicleControls *)p;
}

byte *__fastcall Controls_ctor(byte *self) {
	Controls_Reset(self);
	Ctl(self)->firePrev = 0;
	Ctl(self)->cmPrev = 0;
	return self;
}

void __fastcall Controls_Reset(byte *self) {
	cVehicleControls *c = Ctl(self);

	c->weapon = g_pWorld->firstWeapon;
	c->str = 0.0f;
	c->acc = 0.0f;
	c->vRot = 0.0f;
	c->hRot = 0.0f;
	c->vStr = 0.0f;
	c->cm = 0;
	c->b24 = 0;
	c->b25 = 1;
	c->b26 = 0;
	c->fire = 0;
	c->emptyTime = 0.0f;
	c->netFire = 0;
	c->b28 = 0;
	c->fireWait = 0.0f;
	c->padBounce = 0.0f;
	c->rumbleA = 0.0f;
	c->rumbleB = 0.0f;
	c->rumbleARate = 0.0f;
	c->rumbleBRate = 0.0f;
	c->attackTime = 0.0f;
}

/* a float of the controls from the double variable */
static inline void Ctl_Var(float *f, double *d, int set) {
	if ((byte)set) {
		*f = (float)*d;
	} else {
		*d = *f;
	}
}

byte __thiscall Controls_VarF(byte *self, const char *name, double *d, int set) {
	cVehicleControls *c = Ctl(self);

	if (!strcmp(name, "Acc")) {
		Ctl_Var(&c->acc, d, set);
		return 1;
	}
	if (!strcmp(name, "VStr")) {
		Ctl_Var(&c->vStr, d, set);
		return 1;
	}
	if (!strcmp(name, "Str")) {
		Ctl_Var(&c->str, d, set);
		return 1;
	}
	if (!strcmp(name, "HRot")) {
		Ctl_Var(&c->hRot, d, set);
		return 1;
	}
	if (!strcmp(name, "VRot")) {
		Ctl_Var(&c->vRot, d, set);
		return 1;
	}
	return 0;
}

int __thiscall Controls_GetWeapon(byte *self) {
	return Ctl(self)->weapon;
}

void __thiscall Controls_FromNet(byte *self, cEntity *v, float dt) {
	cVehicleControls *c = Ctl(self);
	const byte *net = *(byte **)((byte *)v + 0xf4);
	uint16_t bits = *(const uint16_t *)(net + 0x74);
	byte toggle = (byte)(bits >> 15) & 1;

	(void)dt;
	if (c->netFire == toggle) {
		c->fire = (byte)bits & 1;
	} else {
		c->fire = 1;
		if (c->firePrev) {
			c->netFire = toggle;
		}
	}
	c->cm = ((byte)bits >> 1) & 1;
	if (bits & 0x100) {
		c->acc = 1.0f;
	} else if (bits & 0x200) {
		c->acc = -1.0f;
	} else {
		c->acc = 0.0f;
	}
	if (bits & 0x800) {
		c->str = 1.0f;
	} else if (bits & 0x400) {
		c->str = -1.0f;
	} else {
		c->str = 0.0f;
	}
}

void __thiscall Controls_SetWeapon(byte *self, int w) {
	cVehicleControls *c = Ctl(self);

	if (c->weapon != w) {
		c->weapon = w;
		c->fire = 0;
		c->cm = 0;
		c->fireWait = 0.0f;
	}
}

void __thiscall Controls_RumbleA(byte *self, float strength, float time) {
	float t = Float_MaxSS(CONTROLS_MIN_FADE, time);

	Ctl(self)->rumbleA = strength;
	Ctl(self)->rumbleARate = strength / t;
}

void __thiscall Controls_RumbleB(byte *self, float strength, float time) {
	float t = Float_MaxSS(CONTROLS_MIN_FADE, time);

	Ctl(self)->rumbleB = strength;
	Ctl(self)->rumbleBRate = strength / t;
}

/* ---- the items a ship takes (native functions "TakeEner", "TakeLife", "TakeMiss", "TakePart") ---- */

/* the engine variables of the items: the ammo or life they add and the time until they come back */
#define g_ItemEnerAdds GAME_GLOBAL_IN(int, GVar_ItemEnerAdds, 16, 0x447b04)
#define g_ItemEnerRespawnTime GAME_GLOBAL_IN(float, GVar_ItemEnerRespawnTime, 16, 0x447a60)
#define g_ItemLifeAdds GAME_GLOBAL_IN(int, GVar_ItemLifeAdds, 16, 0x447b5c)
#define g_ItemLifeRespawnTime GAME_GLOBAL_IN(float, GVar_ItemLifeRespawnTime, 16, 0x447aa0)
#define g_ItemMissAdds GAME_GLOBAL_IN(int, GVar_ItemMissAdds, 16, 0x447a78)
#define g_ItemMissRespawnTime GAME_GLOBAL_IN(float, GVar_ItemMissRespawnTime, 16, 0x447b1c)
#define g_ItemPartAdds GAME_GLOBAL_IN(int, GVar_ItemPartAdds, 16, 0x447b44)
#define g_ItemPartRespawnTime GAME_GLOBAL_IN(float, GVar_ItemPartRespawnTime, 16, 0x447ac8)

/* the arguments: the item and the ship that took it; the ship or NULL when it is no "Car" */
static cEntity *Ctl_TakeArgs(cAbstractFile *f, char *item, char *name) {
	cAbstractFile_ReadString(f, item, 0x80);
	cAbstractFile_ReadString(f, name, 0x80);
	cEntity *v = World_FindEntity(name);
	if (!v || !v->vftable->IsType(v, TYEN_CAR)) {
		return NULL;
	}
	return v;
}

/* ammo of kind (a weapon of kind or alt must be there) up to its maximum, the item hidden for respawn */
static void Ctl_TakeAmmo(cAbstractFile *f, int kind, int alt, int adds, const float *respawn) {
	char item[0x80], name[0x80];
	cEntity *v = Ctl_TakeArgs(f, item, name);

	if (!v) {
		return;
	}
	cWeaponManager *w = NetEnt_Weapons(v);
	if (!cWeaponManager_GetSlots(w, kind) && !cWeaponManager_GetSlots(w, alt)) {
		return;
	}
	const cAmmoInfo *info = TABLE_AT(g_pWorld->ammo, kind);
	if (cTargetLock_GetAmmo(w, kind) >= info->max) {
		return;
	}
	cWeaponManager_SetAmmo(w, kind, cTargetLock_GetAmmo(w, kind) + (float)adds);
	cItemEntity_SetRespawn((cItemEntity *)World_FindEntity(item), *respawn);
}

void Controls_TakeEnerNative(cAbstractFile *f) {
	Ctl_TakeAmmo(f, 2, 4, g_ItemEnerAdds, &g_ItemEnerRespawnTime);
}

void Controls_TakeLifeNative(cAbstractFile *f) {
	char item[0x80], name[0x80];
	cEntity *v = Ctl_TakeArgs(f, item, name);

	if (!v) {
		return;
	}
	cWithLifeEntity *w = WLife(v);
	int max = NetEnt_MaxLife(v);
	if (w->life >= max) {
		return;
	}
	byte god = w->god;
	if (!god) {
		w->life = w->life + g_ItemLifeAdds;
	}
	if (w->life > max && !god) {
		w->life = max;
	}
	if ((byte)cPy_Function_IsValid(g_OnLifeItemTaken)) {
		cPy_Function_AddParamS(g_OnLifeItemTaken, name);
		cPy_Function_Execute(g_OnLifeItemTaken, 0);
	}
	cItemEntity_SetRespawn((cItemEntity *)World_FindEntity(item), g_ItemLifeRespawnTime);
}

void Controls_TakeMissNative(cAbstractFile *f) {
	Ctl_TakeAmmo(f, 1, 5, g_ItemMissAdds, &g_ItemMissRespawnTime);
}

void Controls_TakePartNative(cAbstractFile *f) {
	Ctl_TakeAmmo(f, 0, 3, g_ItemPartAdds, &g_ItemPartRespawnTime);
}

/* ---- the target selection of a player ship (include/scrap/vehicleentity.h) ---- */

#define TGT_WAIT 3.0f				/* after a choice */
#define TGT_VIEW_RANGE 50000.0f
#define TGT_VIEW_COS 0.95f

static inline cVehicleTargets *Tgt(void *o) {
	return o;
}

/* the inlined copies of IsBlocked */
static inline byte Tgt_Blocked(cVehicleTargets *t) {
	if (g_VehicleCenterPadLock != 0.0f && cVehicleEntity_GetOwnerSlot(t->owner) != -1) {
		return 0;
	}
	return WLife_Vtbl(t->owner)->IsEMIed(t->owner) ? 1 : 0;
}

/* the unbounded copies of the original; every name of the list came through Str_Copy */
static inline void Tgt_Copy(char *dst, const char *src) {
	int i = 0;

	do {
		dst[i] = src[i];
	} while (src[i++]);
}

void *__fastcall cVehicleO648_ctor(void *o) {
	cVehicleTargets *t = o;

	t->target[0] = 0;
	t->owner = NULL;
	t->list = NULL;
	return t;
}

void __fastcall cVehicleO648_dtor(void *o) {
	if (Tgt(o)->list) {
		operator_delete_sized(Tgt(o)->list, sizeof(cVehicleTargetList));
	}
}

byte __fastcall cVehicleO648_IsBlocked(void *o) {
	return Tgt_Blocked(Tgt(o));
}

void __fastcall cVehicleO648_Rebuild(void *o) {
	cVehicleTargets *t = o;
	float dist[VEH_TARGETS];
	vec3_t a, b;

	if (!t->list) {
		return;
	}
	t->list->count = 0;
	t->target[0] = 0;
	cWithLifeEntity *ow = WLife(t->owner);
	float limit = g_TgtRegenerationTime + ow->attack.hitTime;
	if (limit > Timer_GetTime(&g_pWorld->timer)) {
		/* the attacker of the last hit first */
		const char *name = Nto1Node_TargetName(&ow->attack.hitBy);
		if (*name) {
			cEntity *e = World_FindEntity(name);
			if (e && e != t->owner && e->vftable->IsType(e, TYEN_WITH_LIFE) &&
				Team_IsEnemy(NetEnt_Team(e), NetEnt_Team(t->owner))) {
				cVehicleTargetList *l = t->list;
				int n = l->count;
				dist[n] = 0.0f;
				l->count = n + 1;
				Str_Copy(l->names[n], Nto1Node_TargetName(&WLife(t->owner)->attack.hitBy), 0x80);
				Str_Copy(t->target, Nto1Node_TargetName(&WLife(t->owner)->attack.hitBy), 0x80);
			}
		}
	}
	/* the enemies by distance, an insertion into the sorted list */
	for (cEntity *e = cWorld_GetFirstEntity(g_pWorld); e; e = cEntity_Next(e)) {
		if (!e->flag_4c || !e->vftable->IsType(e, TYEN_WITH_LIFE)) {
			continue;
		}
		cEntity *own = t->owner;
		if (!strcmp(e->name, own->name) || !Team_IsEnemy(NetEnt_Team(e), NetEnt_Team(own)) ||
			!strcmp(e->name, t->target)) {
			continue;
		}
		const vec3_t *op = t->owner->vftable->GetPos(t->owner, &a);
		const vec3_t *ep = e->vftable->GetPos(e, &b);
		float dx = ep->x - op->x;
		float dy = ep->y - op->y;
		float dz = ep->z - op->z;
		float d2 = dy * dy + dx * dx + dz * dz;
		cVehicleTargetList *l = t->list;
		if (l->count < VEH_TARGETS) {
			l->count++;
		}
		int i;
		for (i = t->list->count - 2; i >= 0; i--) {
			if (!(dist[i] > d2)) {
				break;
			}
			dist[i + 1] = dist[i];
			Tgt_Copy(t->list->names[i + 1], t->list->names[i]);
		}
		dist[i + 1] = d2;
		Str_Copy(t->list->names[i + 1], e->name, 0x80);
	}
	t->list->cur = 0;
	cVehicleTargetList *l = t->list;
	if (l->count > 0) {
		Tgt_Copy(t->target, l->names[l->cur]);
		return;
	}
	t->target[0] = 0;
}

void __thiscall cVehicleO648_SetSlot(void *o, int slot) {
	cVehicleTargets *t = o;

	if (slot == -1) {
		if (t->list) {
			operator_delete_sized(t->list, sizeof(cVehicleTargetList));
			t->list = NULL;
		}
		return;
	}
	if (t->list) {
		return;
	}
	cVehicleTargetList *l = operator_new(sizeof(cVehicleTargetList));
	if (!l) {
		t->list = NULL;
		return;
	}
	l->count = 0;
	l->cur = 0;
	l->time = -g_TgtRegenerationTime;
	l->wait = 0.0f;
	t->list = l;
}

void __thiscall cVehicleO648_Cycle(void *o, byte next) {
	cVehicleTargets *t = o;

	if (!t->list) {
		return;
	}
	if (Tgt_Blocked(t)) {
		t->target[0] = 0;
		return;
	}
	t->list->wait = TGT_WAIT;
	if (cVehicleO648_IsStale(t)) {
		cVehicleO648_Rebuild(t);
	} else {
		cVehicleTargetList *l = t->list;
		if (l->count <= 0) {
			t->target[0] = 0;
			return;
		}
		if (!strcmp(l->names[l->cur], t->target)) {
			if (next) {
				l->cur++;
				if (l->cur >= t->list->count) {
					t->list->cur = 0;
				}
			} else if (--l->cur < 0) {
				t->list->cur = t->list->count - 1;
			}
		}
		Tgt_Copy(t->target, t->list->names[t->list->cur]);
		if (!World_FindEntity(t->target)) {
			cVehicleO648_Rebuild(t);
		}
	}
	t->list->time = Timer_GetTime(&g_pWorld->timer);
}

void __fastcall cVehicleO648_PickInView(void *o) {
	cVehicleTargets *t = o;

	if (!t->list) {
		return;
	}
	if (!Tgt_Blocked(t)) {
		float dot = -1.0f;
		t->list->wait = TGT_WAIT;
		cEntity *e = Entity_FindInView(t->owner, TGT_VIEW_RANGE, &dot, NULL, 0, -2.0f);
		if (dot > TGT_VIEW_COS && e) {
			Str_Copy(t->target, e->name, 0x80);
			return;
		}
	}
	t->target[0] = 0;
}

void __thiscall cVehicleO648_Update(void *o, float dt) {
	cVehicleTargets *t = o;

	if (!t->list) {
		return;
	}
	if (Tgt_Blocked(t)) {
		t->target[0] = 0;
		return;
	}
	/* the target is dropped when it is gone, inactive or of team 3 */
	cEntity *e = World_FindEntity(t->target);
	if (!e || e == t->owner || !e->flag_4c || !e->vftable->IsType(e, TYEN_WITH_LIFE) || NetEnt_Team(e) == 3) {
		t->target[0] = 0;
	}
	if (!t->target[0] && 0.0f >= t->list->wait) {
		t->list->wait = 0.0f;
		t->list->time = 0.0f;
		cVehicleO648_Cycle(t, 1);
		return;
	}
	t->list->wait = t->list->wait - dt;
}

byte __fastcall cVehicleO648_IsStale(void *o) {
	cVehicleTargets *t = o;
	cVehicleTargetList *l = t->list;

	if (!l) {
		return 1;
	}
	float limit = l->time + g_TgtRegenerationTime;
	if (Timer_GetTime(&g_pWorld->timer) > limit) {
		return 1;
	}
	l = t->list;
	int n = l->count;
	if (n == 1) {
		return 1;
	}
	for (int i = 0; i < n; i++) {
		/* the original compares the first two bytes as a short and the rest from the third on */
		uint16_t a, b;
		memcpy(&a, t->target, 2);
		memcpy(&b, l->names[i], 2);
		if (a != b || strcmp(t->target + 2, l->names[i] + 2)) {
			continue;
		}
		if (!l->names[i][0]) {
			return 1;
		}
		cEntity *e = World_FindEntity(l->names[i]);
		if (!e) {
			return 1;
		}
		return !e->flag_4c;
	}
	return 1;
}

/* ---- the controls of a player ship from the input actions (rva 0xffcc0) ---- */

#define g_VehicleBlockSwitchAdvance GAME_GLOBAL_IN(int, GVar_VehicleBlockSwitchAdvance, 16, 0x447bd4)
#define g_VehicleCenterPadBounce GAME_GLOBAL_IN(float, GVar_VehicleCenterPadBounce, 16, 0x447c04)
#define g_VehicleAttackModeMinDist GAME_GLOBAL_IN(float, GVar_VehicleAttackModeMinDist, 16, 0x447bbc)
#define g_VehicleAttackModeMaxDist GAME_GLOBAL_IN(float, GVar_VehicleAttackModeMaxDist, 16, 0x447c34)
#define g_TimeWSwitchNo GAME_GLOBAL_IN(float, GVar_TimeWSwitchNo, 16, 0x447bec)
#define g_XtrTimeWSwitchNo GAME_GLOBAL_IN(float, GVar_XtrTimeWSwitchNo, 16, 0x447c1c)
#define g_AttackerMarkTime GAME_GLOBAL_IN(float, GVar_AttackerMarkTime, 16, 0x447b8c)

/* the actions of the vehicle action set */
#define CTL_LOOK_UP 2
#define CTL_LOOK_DOWN 3
#define CTL_TURN_LEFT 4
#define CTL_TURN_RIGHT 5
#define CTL_ACCEL2 6
#define CTL_BRAKE 7
#define CTL_ACCEL 8
#define CTL_B24 9
#define CTL_STR_LEFT 10
#define CTL_STR_RIGHT 11
#define CTL_VSTR_DOWN 12
#define CTL_VSTR_UP 13
#define CTL_NEXT_WEAP 14
#define CTL_PREV_WEAP 15
#define CTL_FIRE 16
#define CTL_CM 17
#define CTL_STRAFE 18
#define CTL_NEXT_TARGET 19
#define CTL_PREV_TARGET 20
#define CTL_VIEW_TARGET 21
#define CTL_FOLLOW 22
#define CTL_WEAP0 24				/* the weapons Laser .. Inferno */
#define CTL_CAM_DIST 31

#define CTL_FIRE_WAIT 0x1.99999ap-3f		/* 0.2 */

static inline cInputAction *Ctl_Action(cActionSet *set, int i) {
	return TABLE_AT(set->actions, i);
}

static inline float Ctl_Value(cActionSet *set, int i, int slot) {
	return Ctl_Action(set, i)->value[slot];
}

static inline byte Ctl_Pressed(cActionSet *set, int i, int slot) {
	return cInputAction_Pressed(Ctl_Action(set, i), slot);
}

/* a new weapon clears both buttons and the fire wait */
static inline void Ctl_NewWeapon(cVehicleControls *c, int w) {
	c->weapon = w;
	c->fire = 0;
	c->cm = 0;
	c->fireWait = 0.0f;
}

/* the x87 (lim - now) / div of the original */
static inline float Ctl_X87SubDiv(float now, float lim, float div) {
	float r;

	__asm__("fsubrs %2\n\tfdivs %3" : "=t"(r) : "0"(now), "m"(lim), "m"(div));
	return r;
}

static inline float Ctl_Rumble01(float v) {
	return 0.0f >= v ? 0.0f : Float_MinSS(1.0f, v);
}

/* the aim at the target (or the waypoint) of the ship: the angles to it, the accelerator by the distance */
static void Ctl_FollowTarget(cVehicleControls *c, cEntity *v, cEntity *t, byte attack, float vr, float dt) {
	vec3_t a, b, d, rot;

	const vec3_t *tp = t->vftable->GetPos(t, &a);
	const vec3_t *vp = v->vftable->GetPos(v, &b);
	c->attackTime = dt + c->attackTime;
	d.x = vp->x - tp->x;
	d.y = vp->y - tp->y;
	d.z = vp->z - tp->z;
	if (c->acc == 0.0f) {
		if (attack) {
			c->acc = 1.0f;
		} else {
			float len = (float)__builtin_sqrt((double)(d.x * d.x + d.y * d.y + d.z * d.z));
			if (g_VehicleAttackModeMinDist > len) {
				c->acc = -1.0f;
			} else if (len > g_VehicleAttackModeMaxDist) {
				c->acc = 1.0f;
			} else {
				c->acc = 0.0f;
			}
		}
	}
	const vec3_t *ang = Obj_Vtbl(v)->GetAng(v, &b);
	const vec3_t *r = Vec3_GetRotAng(&rot, &d);
	float dy = r->y - ang->y;
	float dx = NormAng(r->x - ang->x);
	dy = NormAng(dy);
	c->vRot = dx + vr;
	c->hRot = c->hRot + dy;
}

/* the sticks, the strafe mode, the follow of the target */
static void Ctl_Axes(cVehicleControls *c, cEntity *v, cActionSet *set, int slot, float dt) {
	vec3_t a;

	byte strafe = Ctl_Value(set, CTL_STRAFE, slot) > 0.5f;
	(void)Ctl_Action(set, CTL_ACCEL2);
	(void)Ctl_Action(set, CTL_BRAKE);
	float acc = Ctl_Value(set, CTL_ACCEL, slot);
	float acc2 = Ctl_Value(set, CTL_ACCEL2, slot);
	if (acc > 0.5f) {
		c->b25 = 0;
	}
	if (acc2 > 0.5f) {
		c->b25 = 1;
	}
	c->acc = c->b25 ? acc2 : acc;
	c->b24 = Ctl_Value(set, CTL_B24, slot) > 0.5f;
	c->acc = c->acc - Ctl_Value(set, CTL_BRAKE, slot);
	c->str = Ctl_Value(set, CTL_STR_LEFT, slot);
	c->str = c->str - Ctl_Value(set, CTL_STR_RIGHT, slot);
	c->vStr = Ctl_Value(set, CTL_VSTR_UP, slot);
	c->vStr = c->vStr - Ctl_Value(set, CTL_VSTR_DOWN, slot);
	c->b28 = Ctl_Value(set, CTL_FOLLOW, slot) > 0.5f;
	if (strafe) {
		c->attackTime = 0.0f;
		c->hRot = 0.0f;
		c->vRot = 0.0f;
		if (g_VehicleBlockSwitchAdvance) {
			c->acc = 0.0f;
		}
		c->str = Ctl_Value(set, CTL_TURN_LEFT, slot) + c->str;
		c->str = c->str - Ctl_Value(set, CTL_TURN_RIGHT, slot);
		c->vStr = Ctl_Value(set, CTL_LOOK_DOWN, slot);
		c->vStr = c->vStr - Ctl_Value(set, CTL_LOOK_UP, slot);
		if (g_VehicleBlockSwitchAdvance) {
			c->vStr = c->vStr - Ctl_Value(set, CTL_ACCEL2, slot);
			c->vStr = Ctl_Value(set, CTL_BRAKE, slot) + c->vStr;
		}
		return;
	}
	c->hRot = Ctl_Value(set, CTL_TURN_RIGHT, slot);
	c->hRot = c->hRot - Ctl_Value(set, CTL_TURN_LEFT, slot);
	float vr = Ctl_Value(set, CTL_LOOK_UP, slot);
	vr = vr - Ctl_Value(set, CTL_LOOK_DOWN, slot);
	if (c->b28) {
		c->padBounce = g_VehicleCenterPadBounce;
	}
	if (slot == -1 || g_VehicleCenterPadLock == 0.0f) {
		c->attackTime = 0.0f;
		c->vRot = vr;
		return;
	}
	cEntity *t = NULL;
	if (!cVehicleO648_IsBlocked(Veh(v)->o648)) {
		t = World_FindEntity(Veh(v)->target);
	}
	byte attack = 0;
	if (!t) {
		/* the waypoint: an entity that is no ship is attacked at full speed */
		const char *n = NULL;
		if (slot == 1 && g_CurrentWaypointName2 && *g_CurrentWaypointName2) {
			n = g_CurrentWaypointName2;
		} else if (g_CurrentWaypointName && *g_CurrentWaypointName) {
			n = g_CurrentWaypointName;
		}
		if (n) {
			t = World_FindEntity(n);
		}
		if (t) {
			attack = !t->vftable->IsType(t, TYEN_CAR);
		}
	}
	if (t && c->b28) {
		Ctl_FollowTarget(c, v, t, attack, vr, dt);
		return;
	}
	vr = vr * 0x1.921fb6p+0f;
	c->attackTime = 0.0f;
	if (vr != 0.0f || c->hRot != 0.0f) {
		c->padBounce = 0.0f;
		c->vRot = vr;
		return;
	}
	/* the pitch goes back to level */
	float pb = c->padBounce;
	float nb = g_VehicleCenterPadBounce;
	if (nb > pb) {
		nb = __builtin_fabsf(c->acc) * dt * g_VehicleCenterPadLock + pb;
	}
	c->padBounce = nb;
	const vec3_t *ang = Obj_Vtbl(v)->GetAng(v, &a);
	c->vRot = (vr - ang->x) * c->padBounce;
}

/* the key of a weapon (its id read from the engine variable) */
static inline void Ctl_WeaponKey(cVehicleControls *c, cWeaponManager *wm, cActionSet *set, int i, int slot,
	const int *id) {
	if (Ctl_Pressed(set, i, slot) && cWeaponManager_GetSlots(wm, *id) && c->weapon != *id) {
		Ctl_NewWeapon(c, *id);
	}
}

/* the weapon switch keys and the camera distance */
static void Ctl_Weapons(cVehicleControls *c, cEntity *v, cActionSet *set, int slot) {
	cWeaponManager *wm = NetEnt_Weapons(v);
	int w;

	if (Ctl_Pressed(set, CTL_NEXT_WEAP, slot) && c->weapon != (w = cWeaponManager_NextWeap(wm, c->weapon))) {
		Ctl_NewWeapon(c, w);
	}
	if (Ctl_Pressed(set, CTL_PREV_WEAP, slot) && c->weapon != (w = cWeaponManager_PrevWeap(wm, c->weapon))) {
		Ctl_NewWeapon(c, w);
	}
	Ctl_WeaponKey(c, wm, set, CTL_WEAP0, slot, &g_LaserIDWeap);
	Ctl_WeaponKey(c, wm, set, CTL_WEAP0 + 1, slot, &g_VulcanIDWeap);
	Ctl_WeaponKey(c, wm, set, CTL_WEAP0 + 2, slot, &g_DevastatorIDWeap);
	Ctl_WeaponKey(c, wm, set, CTL_WEAP0 + 3, slot, &g_TeslaIDWeap);
	Ctl_WeaponKey(c, wm, set, CTL_WEAP0 + 4, slot, &g_ATPCIDWeap);
	Ctl_WeaponKey(c, wm, set, CTL_WEAP0 + 5, slot, &g_SwarmIDWeap);
	Ctl_WeaponKey(c, wm, set, CTL_WEAP0 + 6, slot, &g_InfernoIDWeap);
	if (Ctl_Pressed(set, CTL_CAM_DIST, slot)) {
		float *d = slot == 1 ? &g_VehicleCamDist2 : &g_VehicleCamDist;
		float x = *d;
		if (500.0f >= x) {
			*d = 1000.0f;
		} else if (1000.0f >= x) {
			*d = 1750.0f;
		} else if (1750.0f >= x) {
			*d = 3000.0f;
		} else {
			*d = 500.0f;
		}
	}
}

static void Ctl_EmptyWeapon(cVehicleControls *c, cWeaponManager *wm, float dt) {
	c->fireWait = 0.0f;
	if (c->fire) {
		c->emptyTime = c->emptyTime + dt;
	} else if (c->firePrev) {
		c->emptyTime = c->emptyTime + g_XtrTimeWSwitchNo;
	}
	float a = cWeaponManager_GetWeapAmmo(wm, (uint32_t)c->weapon);
	if (0.0f >= a) {
		if (c->cm) {
			c->emptyTime = c->emptyTime + dt;
		} else if (c->cmPrev) {
			c->emptyTime = c->emptyTime + g_XtrTimeWSwitchNo;
		}
	}
	if (c->emptyTime > g_TimeWSwitchNo) {
		int w = cWeaponManager_NextWeap(wm, (uint32_t)g_LaserIDWeap);
		if (c->weapon != w) {
			Ctl_NewWeapon(c, w);
		}
	}
}

/* the force feedback of the pad: the shake of the ship, the hit and the two motors */
static void Ctl_PadRumble(cVehicleControls *c, cEntity *v, int slot) {
	cVehicleEntity *veh = Veh(v);
	float lim = WLife(v)->attack.hitTime + g_AttackerMarkTime;
	float hit = Ctl_X87SubDiv(Timer_GetTime(&veh->timer), lim, g_AttackerMarkTime);
	byte *pl = veh->player;

	if (*(float *)(pl + 0x220) > 0.025f) {
		float p = *(float *)(pl + 0x224);
		if (p * 30.0f > hit) {
			hit = p * 50.0f;
		}
	}
	float shake = Ctl_Rumble01(veh->shake * 0.5f);
	float b = Ctl_Rumble01(c->rumbleB);
	float h = Ctl_Rumble01(hit);
	float a = Ctl_Rumble01(c->rumbleA);
	cPad_Rumble(&g_Pads[slot], a + h, b + shake, 0.25f);
}

void __thiscall cVehicleControls_Tick(void *controls, cEntity *v, float dt) {
	cVehicleControls *c = controls;
	int slot = cVehicleEntity_GetOwnerSlot(v);
	cActionSet *set = TABLE_AT(g_ActionSets, 1);

	Ctl_Axes(c, v, set, slot, dt);
	c->fire = Ctl_Value(set, CTL_FIRE, slot) > 0.5f;
	Ctl_Weapons(c, v, set, slot);
	if (Ctl_Pressed(set, CTL_NEXT_TARGET, slot)) {
		cVehicleO648_Cycle(Veh(v)->o648, 1);
	}
	if (Ctl_Pressed(set, CTL_PREV_TARGET, slot)) {
		cVehicleO648_Cycle(Veh(v)->o648, 0);
	}
	c->b26 = Ctl_Value(set, CTL_VIEW_TARGET, slot) > 0.5f;
	if (c->b26) {
		cVehicleO648_PickInView(Veh(v)->o648);
	}
	c->b26 = c->b26 || Ctl_Value(set, CTL_NEXT_TARGET, slot) > 0.5f;
	c->cm = Ctl_Value(set, CTL_CM, slot) > 0.5f;
	cWeaponManager *wm = NetEnt_Weapons(v);
	if (c->weapon == wm->curWeap && !(byte)cWeaponManager_HasMinAmmo(wm, (uint32_t)c->weapon)) {
		Ctl_EmptyWeapon(c, wm, dt);
	} else {
		if (c->fire) {
			c->fireWait = CTL_FIRE_WAIT;
		} else if (c->fireWait > 0.0f) {
			c->fire = 1;
			c->fireWait = c->fireWait - dt;
		}
		c->emptyTime = 0.0f;
	}
	if (Ctl_Pressed(set, CTL_FIRE, slot)) {
		c->netFire = !c->netFire;
	}
	if (slot != -1 && Input_GetActionSet() == 1 && Veh(v)->player) {
		Ctl_PadRumble(c, v, slot);
	}
	float ra = c->rumbleA - c->rumbleARate * dt;
	c->rumbleA = ra;
	if (0.0f >= ra) {
		c->rumbleA = 0.0f;
	}
	float rb = c->rumbleB - c->rumbleBRate * dt;
	c->rumbleB = rb;
	if (0.0f >= rb) {
		c->rumbleB = 0.0f;
	}
}
