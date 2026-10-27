/*
 * The controls of a vehicle, see include/scrap/controls.h.
 */
#include <string.h>

#include "scrap/controls.h"
#include "scrap/itementity.h"
#include "scrap/mathlib.h"
#include "scrap/net.h"
#include "scrap/timer.h"
#include "scrap/vehicleentity.h"
#include "scrap/weapons.h"
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
	c->i30 = 0;
	c->netFire = 0;
	c->b28 = 0;
	c->i18 = 0;
	c->i1c = 0;
	c->rumbleA = 0.0f;
	c->rumbleB = 0.0f;
	c->rumbleARate = 0.0f;
	c->rumbleBRate = 0.0f;
	c->i2c = 0;
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
		c->i18 = 0;
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
#define g_ItemEnerAdds GAME_DATA(int, 0x447b04)
#define g_ItemEnerRespawnTime GAME_DATA(float, 0x447a60)
#define g_ItemLifeAdds GAME_DATA(int, 0x447b5c)
#define g_ItemLifeRespawnTime GAME_DATA(float, 0x447aa0)
#define g_ItemMissAdds GAME_DATA(int, 0x447a78)
#define g_ItemMissRespawnTime GAME_DATA(float, 0x447b1c)
#define g_ItemPartAdds GAME_DATA(int, 0x447b44)
#define g_ItemPartRespawnTime GAME_DATA(float, 0x447ac8)

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
