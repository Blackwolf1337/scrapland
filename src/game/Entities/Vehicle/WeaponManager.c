/*
 * Entities/Vehicle/WeaponManager.cpp of the game: the weapons of a vehicle, see include/scrap/weapons.h. The world
 * keeps the weapon and ammo tables (g_pWorld->weapons, ->ammo), the manager the mounted slots, upgrades and ammo of
 * its vehicle, the stamps of the next shot and the lock on an entity or a point.
 * The constructor, the destructor and the small methods before rva 0x11a0f0 sit behind VehicleTraffic.cpp in the exe.
 */
#include "scrap/console.h"
#include "scrap/crt.h"
#include "scrap/scrapmodel.h"
#include "scrap/weapons.h"

#include <string.h>

#define WEAPS (g_pWorld->weapons)
#define AMMOS (g_pWorld->ammo)

/* the ammo info of the ammo kind of weapon w */
#define WEAP_AMMOINFO(w) TABLE_AT(AMMOS, TABLE_AT(WEAPS, (w))->ammo)

/* the weapon is mounted, or in a single player game every weapon counts */
static inline int WM_Usable(const cWeaponManager *self, int w) {
	return self->weaps[w].slots != 0 || (g_SplitScreenGame == 0 && g_pApp->flags == 0);
}

static inline int WM_IsDigit(char c) {
	return c >= '0' && c <= '9';
}

int __thiscall cWeaponManager_HasMinAmmo(cWeaponManager *self, uint32_t w) {
	CHECK(g_pWorld->weapons.count > (int)w);
	const cWeapInfo *info = TABLE_AT(WEAPS, w);
	const cWeapInfo *info2 = TABLE_AT(WEAPS, w);
	return self->ammo[info->ammo] > info2->minAmmo;
}

byte __thiscall cWeaponManager_VarF(cWeaponManager *self, const char *name, double *v, byte set) {
	size_t len = strlen(name);
	int i;

	if (len == 7 && WM_IsDigit(name[5]) && WM_IsDigit(name[6]) && !memcmp(name, "UAmmo", 5)) {
		i = crt_atoi(name + 5);
		if (i >= g_pWorld->weapons.count) {
			return 0;
		}
		if (set) {
			cWeaponManager_SetUAmmo(self, i, (float)*v);
		} else {
			*v = cWeaponManager_GetUAmmo(self, i);
		}
		return 1;
	}
	if (!strcmp(name, "iUAmmo")) {
		if (set) {
			cWeaponManager_SetUAmmo(self, self->curWeap, (float)*v);
		} else {
			*v = cWeaponManager_GetUAmmo(self, self->curWeap);
		}
		return 1;
	}
	if (!strcmp(name, "AcUAmmo")) {
		cVehicleControls *ctl = Vehicle_Controls(self->owner);
		if (set) {
			float f = (float)*v;
			cWeaponManager_SetUAmmo(self, Controls_GetWeapon(ctl), f);
		} else {
			*v = cWeaponManager_GetUAmmo(self, Controls_GetWeapon(ctl));
		}
		return 1;
	}
	if (len == 6 && WM_IsDigit(name[4]) && WM_IsDigit(name[5]) && !memcmp(name, "Ammo", 4)) {
		i = crt_atoi(name + 4);
		if (i >= g_pWorld->ammo.count) {
			return 0;
		}
		if (set) {
			cWeaponManager_SetAmmo(self, i, (float)*v);
		} else {
			*v = cTargetLock_GetAmmo(self, i);
		}
		return 1;
	}
	if (!strcmp(name, "Ammoi")) {
		i = TABLE_AT(WEAPS, self->curWeap)->ammo;
		if (set) {
			if (i < g_pWorld->ammo.count) {
				self->ammo[i] = (float)*v;
			}
		} else if (i >= g_pWorld->ammo.count) {
			*v = 0.0;
		} else {
			*v = self->ammo[i];
		}
		return 1;
	}
	if (!strcmp(name, "AmmoAc")) {
		cVehicleControls *ctl = Vehicle_Controls(self->owner);
		if (set) {
			float f = (float)*v;
			cWeaponManager_SetAmmo(self, TABLE_AT(WEAPS, Controls_GetWeapon(ctl))->ammo, f);
		} else {
			*v = cTargetLock_GetAmmo(self, TABLE_AT(WEAPS, Controls_GetWeapon(ctl))->ammo);
		}
		return 1;
	}
	if (!strcmp(name, "FireStamp")) {
		if (set) {
			self->fireStamp = (float)*v;
		} else {
			*v = self->fireStamp;
		}
		return 1;
	}
	if (!strcmp(name, "CMStamp")) {
		if (set) {
			self->cmStamp = (float)*v;
		} else {
			*v = self->cmStamp;
		}
		return 1;
	}
	return 0;
}

float __thiscall cTargetLock_GetAmmo(cTargetLock *self, int ammo) {
	if (ammo >= g_pWorld->ammo.count) {
		return 0.0f;
	}
	return self->ammo[ammo];
}

/* GetFixedWeapon: the laser for a weapon out of the table */
int cWeaponManager_FixWeapon(int w) {
	if (w < 0 || w >= g_pWorld->weapons.count) {
		Con_Bluef("ERROR: GetFixedWeapon() Weap = %d\n", w);
		w = g_LaserIDWeap;
	}
	return w;
}

char *__thiscall cTargetLock_GetEntity(cTargetLock *self) {
	if (self->on && self->lock.target) {
		return ((cEntity *)self->lock.target)->name;
	}
	return NULL;
}

vec3_t *__thiscall cTargetLock_GetPoint(cTargetLock *self) {
	if (self->on && !self->lock.target) {
		return &self->point;
	}
	return NULL;
}

int __thiscall cWeaponManager_NumAmmoSlots(cWeaponManager *self, int ammo) {
	if (ammo >= g_pWorld->ammo.count) {
		return 0;
	}
	const int *n = self->nodes[ammo];
	return (n[0] != -1) + (n[1] != -1) + (n[2] != -1) + (n[3] != -1);
}

/* the mounted slots of weapon w; warn: no slot is an error, for the laser it gets slot 0 */
int __thiscall cWeaponManager_NumSlots(cWeaponManager *self, int w, byte warn) {
	int n;
	if (w < g_pWorld->weapons.count || self->weaps[w].slots != 0) {
		uint32_t m = self->weaps[w].slots;
		n = ((m & 1) != 0) + ((m & 2) != 0) + ((m & 4) != 0) + ((m & 8) != 0);
	} else {
		n = 0;
	}
	if (warn && n == 0) {
		if (w == g_LaserIDWeap) {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-extra-args"
			Con_Warnf("WARNING:GetNumWeaps Laser = No Slot. Set to default.\n", w);
#pragma GCC diagnostic pop
			self->weaps[w].slots = 1;
			return n;
		}
		Con_Bluef("ERROR:GetNumWeaps Weapon %i = No Slot\n", w);
	}
	return n;
}

float __thiscall cWeaponManager_GetWeapAmmo(cWeaponManager *self, uint32_t w) {
	CHECK(g_pWorld->weapons.count > (int)w);
	return self->ammo[TABLE_AT(WEAPS, w)->ammo];
}

uint32_t __thiscall cWeaponManager_GetSlots(cWeaponManager *self, int w) {
	if (w >= g_pWorld->weapons.count) {
		return 0;
	}
	return self->weaps[w].slots;
}

/* the slot masks of all weapons as "m0,m1,..." in a static buffer */
char *__thiscall cWeaponManager_GetWeapList(cWeaponManager *self) {
	char *buf = g_WeapListBuf;
	int pos = 0;

	if (g_pWorld->weapons.count <= 0) {
		return buf;
	}
	for (int i = 0; i < g_pWorld->weapons.count; i++) {
		char *p = buf + pos;
		Str_PrintfQuiet(p, 0x200 - pos, "%d,", self->weaps[i].slots);
		pos += strlen(p);
		CHECK(pos < 511);
	}
	if (pos) {
		pos--;
		if ((uint32_t)pos >= 0x200) {
			crt_report_rangecheckfailure();
		}
		buf[pos] = 0;
	}
	return buf;
}

byte __thiscall cTargetLock_HasSlot(cTargetLock *self, int weap, int slot) {
	if (slot > 4 || weap >= g_pWorld->weapons.count) {
		return 0;
	}
	return (self->weaps[weap].slots & (1u << (slot & 31))) != 0;
}

int __thiscall cWeaponManager_GetUpgrade(cWeaponManager *self, int w) {
	if (w < g_pWorld->weapons.count && WM_Usable(self, w)) {
		return self->weaps[w].upgrade;
	}
	return 0;
}

float __thiscall cWeaponManager_GetUAmmo(cWeaponManager *self, int w) {
	if (w < g_pWorld->weapons.count && WM_Usable(self, w)) {
		return self->weaps[w].uammo;
	}
	return 0.0f;
}

void __thiscall cWeaponManager_Init(cWeaponManager *self, cEntity *owner) {
	CHECK(self->owner == NULL);
	self->owner = owner;
	uint32_t nw = g_pWorld->weapons.count * sizeof(cWeaponSlot);
	uint32_t na = g_pWorld->ammo.count * sizeof(float);
	self->weaps = Mem_Alloc(nw);
	self->ammo = Mem_Alloc(na);
	memset(self->weaps, 0, nw);
	memset(self->ammo, 0, na);
	self->nodes = Mem_Alloc(g_pWorld->ammo.count * sizeof(self->nodes[0]));
	for (int i = 0; i < g_pWorld->ammo.count; i++) {
		self->nodes[i][0] = -1;
		self->nodes[i][1] = -1;
		self->nodes[i][2] = -1;
		self->nodes[i][3] = -1;
	}
	int w = g_pWorld->firstWeapon;
	if (w >= g_pWorld->weapons.count) {
		w = 0;
	}
	self->curWeap = w;
	self->prevWeap = w;
	Controls_SetWeapon(Vehicle_Controls(self->owner), w);
	self->fireStamp = 0.0f;
	self->cmStamp = 0.0f;
	self->weapSnd = -1;
}

/* "<name>NN" with two digits at 5 and 6 */
static inline int WM_Name7(const char *name, size_t len, const char *prefix) {
	return len == 7 && WM_IsDigit(name[5]) && WM_IsDigit(name[6]) && !memcmp(name, prefix, 5);
}

byte __thiscall cWeaponManager_VarI(cWeaponManager *self, const char *name, int *v, byte set) {
	size_t len = strlen(name);
	int i;

	if (!strcmp(name, "WeapSnd")) {
		if (set) {
			cWeaponManager_SetWeapSnd(self, *v);
		} else {
			*v = self->weapSnd;
		}
		return 1;
	}
	if (WM_Name7(name, len, "Upgrd")) {
		i = crt_atoi(name + 5);
		if (i >= g_pWorld->weapons.count) {
			return 0;
		}
		if (set) {
			cWeaponManager_SetUpgrade(self, i, *v);
		} else {
			*v = cWeaponManager_GetUpgrade(self, i);
		}
		return 1;
	}
	if (!set && !strcmp(name, "iWeaponNeed")) {
		*v = cWeaponManager_WeaponNeed(self);
		return 1;
	}
	if (!strcmp(name, "AcUpgrd")) {
		cVehicleControls *ctl = Vehicle_Controls(self->owner);
		if (set) {
			cWeaponManager_SetUpgrade(self, Controls_GetWeapon(ctl), *v);
		} else {
			*v = cWeaponManager_GetUpgrade(self, Controls_GetWeapon(ctl));
		}
		return 1;
	}
	if (!strcmp(name, "iUpgrd")) {
		if (set) {
			cWeaponManager_SetUpgrade(self, self->curWeap, *v);
		} else {
			*v = cWeaponManager_GetUpgrade(self, self->curWeap);
		}
		return 1;
	}
	if (WM_Name7(name, len, "isWpn")) {
		i = crt_atoi(name + 5);
		if (i >= g_pWorld->weapons.count) {
			return 0;
		}
		if (set) {
			cWeaponManager_SetSlots(self, i, *v);
		} else {
			*v = cWeaponManager_GetSlots(self, i);
		}
		return 1;
	}
	if (!set && WM_Name7(name, len, "nWeap")) {
		i = crt_atoi(name + 5);
		if (i >= g_pWorld->weapons.count) {
			return 0;
		}
		*v = cWeaponManager_NumSlots(self, i, 0);
		return 1;
	}
	if (!set && WM_Name7(name, len, "nSlot")) {
		i = crt_atoi(name + 5);
		if (i >= g_pWorld->ammo.count) {
			return 0;
		}
		*v = cWeaponManager_NumAmmoSlots(self, i);
		return 1;
	}
	/* "WeapXNN": slot X (A..D) of weapon NN, the number is not checked */
	if (len == 7 && name[4] >= 'A' && name[4] <= 'D' && WM_IsDigit(name[5]) && WM_IsDigit(name[6])
		&& !memcmp(name, "Weap", 4)) {
		int slot = name[4] - 'A';
		i = crt_atoi(name + 5);
		if (set) {
			cWeaponManager_SetSlot(self, i, slot, *v != 0);
		} else {
			*v = cTargetLock_HasSlot(self, i, slot) != 0;
		}
		return 1;
	}
	if (!set && !strcmp(name, "AcAmmo")) {
		*v = TABLE_AT(WEAPS, Controls_GetWeapon(Vehicle_Controls(self->owner)))->ammo;
		return 1;
	}
	if (!strcmp(name, "AcWeap")) {
		cVehicleControls *ctl = Vehicle_Controls(self->owner);
		if (set) {
			Controls_SetWeapon(ctl, *v);
		} else {
			*v = Controls_GetWeapon(ctl);
		}
		return 1;
	}
	if (set) {
		return 0;
	}
	if (!strcmp(name, "iWeap")) {
		*v = self->curWeap;
		return 1;
	}
	if (!strcmp(name, "iAmmo")) {
		*v = TABLE_AT(WEAPS, self->curWeap)->ammo;
		return 1;
	}
	return 0;
}

/* the next weapon after w in the order of iNext (iPrev) that is mounted and has its minimum ammo, else w */
int __thiscall cWeaponManager_NextWeap(cWeaponManager *self, uint32_t w) {
	uint32_t i = w;
	do {
		i = TABLE_AT(WEAPS, i)->next;
		if ((int)i < g_pWorld->weapons.count && self->weaps[i].slots != 0 && cWeaponManager_HasMinAmmo(self, i)) {
			break;
		}
	} while (i != w);
	return i;
}

int __thiscall cWeaponManager_PrevWeap(cWeaponManager *self, uint32_t w) {
	uint32_t i = w;
	do {
		i = TABLE_AT(WEAPS, i)->prev;
		if ((int)i < g_pWorld->weapons.count && self->weaps[i].slots != 0 && cWeaponManager_HasMinAmmo(self, i)) {
			break;
		}
	} while (i != w);
	return i;
}

float __thiscall cWeaponManager_SetAmmo(cWeaponManager *self, int ammo, float v) {
	if (ammo >= g_pWorld->ammo.count) {
		return 0.0f;
	}
	self->ammo[ammo] = v;
	return v;
}

/* the lock on the entity of that name (the name may be empty); NULL switches the lock off */
void __thiscall cWeaponManager_LockEntity(cWeaponManager *self, const char *name) {
	if (name) {
		Nto1Node_LinkByName(&self->lock, name);
		self->on = 1;
		return;
	}
	self->on = 0;
}

void __thiscall cWeaponManager_LockPoint(cWeaponManager *self, const vec3_t *p) {
	if (p) {
		CHECK(self->lock.data);
		Nto1Node_Unlink(&self->lock);
		self->point = *p;
		self->on = 1;
		return;
	}
	self->on = 0;
}

void __thiscall cWeaponManager_SetFlag46(cWeaponManager *self, byte v) {
	self->flag46 = v;
}

/* the slots of weapon w from the bits of mask, up to the first slot without node; the new slots */
uint32_t __thiscall cWeaponManager_SetSlots(cWeaponManager *self, int w, int mask) {
	if (w >= g_pWorld->weapons.count) {
		return 0;
	}
	uint32_t bit = 1;
	for (int i = 0; i <= 4 && w < g_pWorld->weapons.count; ) {
		byte on = mask & 1;
		if (self->nodes[TABLE_AT(WEAPS, w)->ammo][i] == -1) {
			break;
		}
		if (on) {
			self->weaps[w].slots |= bit;
		} else {
			self->weaps[w].slots &= ~bit;
		}
		mask >>= 1;
		i++;
		bit <<= 1;
		if (i >= 4) {
			break;
		}
	}
	return self->weaps[w].slots;
}

/* the slot masks from "m0,m1,..." (the last one also sets the slots one by one); the laser keeps slot 0 */
void __thiscall cWeaponManager_SetWeapList(cWeaponManager *self, const char *list) {
	char buf[64];
	int i = 0;

	if (g_pWorld->weapons.count > 0) {
		do {
			if (!*list) {
				break;
			}
			const char *comma = strchr(list, ',');
			if (!comma) {
				int m = crt_atoi(list);
				if (i < g_pWorld->weapons.count) {
					for (int s = 0; s < 4; s++) {
						if (!cWeaponManager_SetSlot(self, i, s, m & 1)) {
							break;
						}
						m >>= 1;
					}
				}
				break;
			}
			uint32_t n = comma - list;
			if (n >= sizeof(buf)) {
				crt_report_rangecheckfailure();
			}
			memcpy(buf, list, n);
			buf[n] = 0;
			cWeaponManager_SetSlots(self, i, crt_atoi(buf));
			list = comma + 1;
			i++;
		} while (i < g_pWorld->weapons.count);
	}
	cWeaponManager_SetSlots(self, g_LaserIDWeap, 1);
}

byte __thiscall cWeaponManager_SetSlot(cWeaponManager *self, int w, int slot, byte on) {
	if (slot > 4 || w >= g_pWorld->weapons.count) {
		return 0;
	}
	uint32_t bit = 1u << (slot & 31);
	if (self->nodes[TABLE_AT(WEAPS, w)->ammo][slot] == -1) {
		return 0;
	}
	if (on) {
		self->weaps[w].slots |= bit;
	} else {
		self->weaps[w].slots &= ~bit;
	}
	return 1;
}

/* 0 always, else an upgrade 1..15 the weapon has */
byte __thiscall cWeaponManager_SetUpgrade(cWeaponManager *self, int w, int upgrade) {
	if (w >= g_pWorld->weapons.count) {
		return 0;
	}
	if (upgrade == 0) {
		self->weaps[w].upgrade = 0;
		return 1;
	}
	if (WM_Usable(self, w) && upgrade >= 0 && upgrade < 16 && TABLE_AT(WEAPS, w)->upgrade[upgrade - 1]) {
		self->weaps[w].upgrade = upgrade;
		return 1;
	}
	return 0;
}

byte __thiscall cWeaponManager_SetUAmmo(cWeaponManager *self, int w, float v) {
	if (w >= g_pWorld->weapons.count || !WM_Usable(self, w)) {
		return 0;
	}
	if (0.0f >= v) {
		self->weaps[w].uammo = 0.0f;
	} else if (v >= 200.0f) {
		self->weaps[w].uammo = 200.0f;
	} else {
		self->weaps[w].uammo = v;
	}
	return 1;
}

/* the looping sound snd of the owner while it fires with ammo, else none */
void __thiscall cWeaponManager_SetWeapSnd(cWeaponManager *self, int snd) {
	cEntity *owner = self->owner;
	int a = TABLE_AT(WEAPS, self->curWeap)->ammo;
	if (!(a < g_pWorld->ammo.count && !(0.0f >= self->ammo[a]) && Vehicle_Controls(owner)->fire)) {
		snd = -1;
	}
	if (snd == self->weapSnd) {
		return;
	}
	cSndItem *it = PtrArray_Get(Vehicle_Sounds(self->owner), self->weapSnd);
	if (it && it->sound.desc.loop) {
		it->volume = 0.0f;
		it->pitch = 0.0f;
	}
	self->weapSnd = snd;
	Vehicle_StartSnd(self->owner, snd);
}

/* the weapon callbacks of the frame: CM and fire press and release, the ammo charge, OnFire, the weapon change */
void __thiscall cWeaponManager_Update(cWeaponManager *self, float dt) {
	cEntity *owner = self->owner;
	cVehicleControls *ctl = Vehicle_Controls(owner);
	const char *name;

	if (!CGameModel_IsLoaded(WEnt_Vtbl(owner)->GetObjData(owner))) {
		return;
	}
	byte net = Net_IsNetEntity(self->owner);
	byte changed = self->curWeap != Controls_GetWeapon(ctl);
	if (changed) {
		ctl->fire = 0;
		ctl->cm = 0;
	}
	if (self->curWeap < 0 || self->curWeap >= g_pWorld->weapons.count) {
		Con_Bluef("ERROR: cWeaponManager::Slot() AcWeap = %d\n", self->curWeap);
		self->curWeap = g_LaserIDWeap;
		Controls_SetWeapon(ctl, self->curWeap);
	}
	if (ctl->cm != ctl->cmPrev) {
		name = self->owner->name;
		if (ctl->cm) {
			cPy_Function_AddParamS(&WEAP_AMMOINFO(self->curWeap)->onPress, name);
			cPy_Function_Execute(&WEAP_AMMOINFO(self->curWeap)->onPress, 0);
		} else {
			cPy_Function_AddParamS(&WEAP_AMMOINFO(self->curWeap)->onRelease, name);
			cPy_Function_Execute(&WEAP_AMMOINFO(self->curWeap)->onRelease, 0);
		}
	}
	ctl->cmPrev = ctl->cm;
	if (ctl->fire != ctl->firePrev) {
		name = self->owner->name;
		if (ctl->fire) {
			cPy_Function_AddParamS(&TABLE_AT(WEAPS, self->curWeap)->onPress, name);
			cPy_Function_Execute(&TABLE_AT(WEAPS, self->curWeap)->onPress, 0);
		} else {
			cPy_Function_AddParamS(&TABLE_AT(WEAPS, self->curWeap)->onRelease, name);
			cPy_Function_Execute(&TABLE_AT(WEAPS, self->curWeap)->onRelease, 0);
		}
	}
	ctl->firePrev = ctl->fire;

	float fs = dt + self->fireStamp;
	self->fireStamp = fs;
	byte ready = !(0.0f > fs);
	if (!net) {
		for (int i = 0; i < g_pWorld->ammo.count; i++) {
			self->ammo[i] = TABLE_AT(AMMOS, i)->charger * dt + self->ammo[i];
			if (self->ammo[i] > TABLE_AT(AMMOS, i)->max) {
				self->ammo[i] = TABLE_AT(AMMOS, i)->max;
			}
		}
	}
	int a = TABLE_AT(WEAPS, self->curWeap)->ammo;
	if (ctl->fire && self->fireStamp >= 0.0f && self->ammo[a] > 0.0f && Vehicle_CanFire(self->owner)) {
		name = self->owner->name;
		cPy_Function_AddParamS(&TABLE_AT(WEAPS, self->curWeap)->onFire, name);
		cPy_Function_Execute(&TABLE_AT(WEAPS, self->curWeap)->onFire, 0);
	}
	float cs = self->cmStamp + dt;
	self->cmStamp = cs;
	byte sw = ready;
	if (0.0f > cs) {
		sw = 0;
	}
	if (ctl->cm && cs >= 0.0f && self->ammo[a] > 0.0f) {
		name = self->owner->name;
		cPy_Function_AddParamS(&TABLE_AT(AMMOS, a)->onFire, name);
		cPy_Function_Execute(&TABLE_AT(AMMOS, a)->onFire, 0);
	}
	cAutoAim_Update(Vehicle_AutoAim(self->owner), self->owner, dt);
	cVehicleAttack_Update(Vehicle_Attack(self->owner));

	/* sw: a net vehicle switches (prevWeap is set after OnEnd), else the stamps allow the change */
	if (net) {
		int slot = Vehicle_NetSlot(self->owner);
		if (slot < 0 || slot >= 4) {
			Controls_GetWeapon(ctl);
			Controls_SetWeapon(ctl, self->curWeap);
		} else if (self->prevWeap == self->curWeap) {
			sw = 0;
		} else {
			sw = 1;
			if (self->prevWeap < 0 || self->prevWeap >= g_pWorld->weapons.count) {
				Con_Bluef("PrevAcWeap = vc->GetPrevAcWeap();\nERROR: cWeaponManager::Slot() PrevAcWeap = %d\n",
					self->prevWeap);
				self->prevWeap = 0;
			}
			goto change;
		}
	}
	if (!changed || !sw) {
		goto hook;
	}
	self->prevWeap = self->curWeap;
	sw = 0;
change:
	name = self->owner->name;
	cPy_Function_AddParamS(&TABLE_AT(WEAPS, self->prevWeap)->onEnd, name);
	cPy_Function_Execute(&TABLE_AT(WEAPS, self->prevWeap)->onEnd, 0);
	if (sw) {
		self->prevWeap = self->curWeap;
	} else {
		self->curWeap = Controls_GetWeapon(ctl);
		if (self->curWeap < 0 || self->curWeap >= g_pWorld->weapons.count) {
			Con_Bluef("AcWeap = vc->GetAcWeap();\nERROR: cWeaponManager::Slot() AcWeap = %d\n", self->curWeap);
			self->curWeap = g_LaserIDWeap;
			Controls_SetWeapon(ctl, self->curWeap);
		}
	}
	name = self->owner->name;
	cPy_Function_AddParamS(&TABLE_AT(WEAPS, self->curWeap)->onStart, name);
	cPy_Function_Execute(&TABLE_AT(WEAPS, self->curWeap)->onStart, 0);
	Vehicle_StartSnd(self->owner, 0x14);
hook:
	if (self->wasOn != self->on) {
		cPy_Function_AddParamS(g_HookPlugFunc, self->owner->name);
		cPy_Function_AddParamI(g_HookPlugFunc, self->on != 0);
		cPy_Function_Execute(g_HookPlugFunc, 0);
	}
	if (self->curWeap != g_LaserIDWeap || !ctl->cm) {
		cSndItem *it = PtrArray_Get(Vehicle_Sounds(self->owner), 0x18);
		if (it && it->playing) {
			it->volume = 0.0f;
			it->pitch = 0.0f;
		}
	}
	self->wasOn = self->on;
	if (g_pApp->flags == 0 && g_SplitScreenGame == 0) {
		for (int i = 0; i < g_pWorld->weapons.count; i++) {
			self->weaps[i].uammo = 200.0f;
		}
	}
}

/* a sound sequence: when the weapon sound ran out its time, the next one of its range starts with the rest */
void __thiscall cWeaponManager_NextWeapSnd(cWeaponManager *self, int unused) {
	(void)unused;
	cWeaponManager_SetWeapSnd(self, self->weapSnd);
	cSndItem *it = PtrArray_Get(Vehicle_Sounds(self->owner), self->weapSnd);
	if (!it || it->sound.desc.loop || !(it->times[0] > 0.0f) || !(it->time > it->times[0])) {
		return;
	}
	int hi = it->ints[1];
	int r = crt_rand();
	int lo = it->ints[0];
	int d = lo - r % (hi + 1);
	if (d == 0) {
		d = lo;
		if (d == 0) {
			d = -hi;
		}
	}
	int next = d + self->weapSnd;
	cSndItem *n = PtrArray_Get(Vehicle_Sounds(self->owner), next);
	float rest = it->time - it->times[0];
	if (rest > it->times[0]) {
		n->time = 0.0f;
	} else {
		n->time = rest;
	}
	if (self->weapSnd == next) {
		self->weapSnd = -1;
	}
	cWeaponManager_SetWeapSnd(self, next);
}

byte __thiscall cWeaponManager_VarS(cWeaponManager *self, const char *name, char **v, byte set) {
	if (strcmp(name, "WeapList")) {
		return 0;
	}
	if (!set) {
		*v = cWeaponManager_GetWeapList(self);
		return 1;
	}
	cWeaponManager_SetWeapList(self, *v);
	if (self->owner) {
		void *ai = self->owner->vftable->GetAI(self->owner);
		if (ai) {
			cAIVehicle_WeapListChanged(ai);
		}
	}
	return 1;
}

/* "WPosXNN": the position of the render node of slot X (A..D) of weapon NN, read only */
byte __thiscall cWeaponManager_VarV(cWeaponManager *self, const char *name, double *x, double *y, double *z,
	byte set) {
	size_t len = strlen(name);
	if (set || len != 7 || !(name[4] >= 'A' && name[4] <= 'D') || !WM_IsDigit(name[5]) || !WM_IsDigit(name[6])
		|| memcmp(name, "WPos", 4)) {
		return 0;
	}
	int slot = name[4] - 'A';
	int w = crt_atoi(name + 5);
	if (w >= g_pWorld->weapons.count) {
		return 0;
	}
	int node = self->nodes[TABLE_AT(WEAPS, w)->ammo][slot];
	if (node == -1) {
		return 0;
	}
	matrix_t m;
	CGameModel *model = WEnt_Vtbl(self->owner)->GetObjData(self->owner);
	const matrix_t *r = CGameModel_NodeMatrix(model, &m, node, 0, NULL, 0);
	float fx = r->m[12];
	float fy = r->m[13];
	float fz = r->m[14];
	*x = fx;
	*y = fy;
	*z = fz;
	return 1;
}

void __thiscall cWeaponManager_PlayStartSnd(cWeaponManager *self) {
	Vehicle_StartSnd(self->owner, 0x14);
}

/* the ammo kind of a mounted weapon below its maximum, in one of three random orders; 0 for none */
int __thiscall cWeaponManager_WeaponNeed(cWeaponManager *self) {
	if (g_pWorld->weapons.count <= 6) {
		return 0;
	}
	const int *order = g_WeaponNeedOrder[crt_rand() % 3];
	for (int i = 0; i < 6; i++) {
		int w = order[i];
		if (self->weaps[w].slots != 0) {
			int a = TABLE_AT(WEAPS, w)->ammo;
			if (TABLE_AT(AMMOS, a)->max > self->ammo[a]) {
				return a;
			}
		}
	}
	return 0;
}

/* the functions before rva 0x11a0f0 */

cWeaponManager *__fastcall cWeaponManager_ctor(cWeaponManager *self) {
	self->vftable = CWEAPONMANAGER_VFTABLE;
	self->lock.target = NULL;
	self->lock.data = NULL;
	self->lock.next = &self->lock;
	self->lock.prev = &self->lock;
	self->owner = NULL;
	self->on = 0;
	self->wasOn = 0;
	self->flag46 = 0;
	self->lock.data = self;
	return self;
}

void __fastcall cWeaponManager_dtor(cWeaponManager *self) {
	self->vftable = CWEAPONMANAGER_VFTABLE;
	Mem_Free(self->weaps);
	Mem_Free(self->ammo);
	Mem_Free(self->nodes);
	Nto1Node_Done(&self->lock);
}

/* Table<cWeapInfo>::operator[] (the world weapons) */
cWeapInfo *__thiscall WeapTable_At(void *table, uint32_t i) {
	return TABLE_AT(*(TABLE(cWeapInfo) *)table, i);
}

cWeaponManager *__thiscall cWeaponManager_Delete(cWeaponManager *self, byte flags) {
	cWeaponManager_dtor(self);
	if (flags & 1) {
		operator_delete_sized(self, sizeof(cWeaponManager));
	}
	return self;
}

void __thiscall cTargetLock_SetHookDelay(cTargetLock *self, float t) {
	if (self->fireStamp > t) {
		self->cmStamp = -t;
	} else {
		self->cmStamp = self->cmStamp - t;
	}
}

void __thiscall cWeaponManager_DelayFire(cWeaponManager *self, float t) {
	if (self->fireStamp > t) {
		self->fireStamp = -t;
	} else {
		self->fireStamp = self->fireStamp - t;
	}
}

void __fastcall cWeaponManager_ClearAmmo(cWeaponManager *self) {
	memset(self->ammo, 0, g_pWorld->ammo.count * 4);
}

void __fastcall cWeaponManager_ClearWeaps(cWeaponManager *self) {
	memset(self->weaps, 0, g_pWorld->weapons.count * sizeof(cWeaponSlot));
}
