/*
 * cNetVController of the game (rva 0x1186b0..0x119850, between VehicleMotorsEffects.cpp and VehicleTraffic.cpp; the
 * source file has no CHECK string, its name is not known): the network controller of a vehicle, a cNetController
 * (ObjEntity.c) with the vehicle, the hits and the controls of the client. See include/scrap/vehicleentity.h.
 */
#include "scrap/objentity.h"
#include "scrap/vehicleentity.h"
#include "scrap/camcontrol.h"
#include "scrap/collisiongrid.h"
#include "scrap/controls.h"
#include "scrap/crt.h"
#include "scrap/filesys.h"
#include "scrap/fxweapons.h"
#include "scrap/game.h"
#include "scrap/pyfunction.h"
#include "scrap/weapons.h"
#include "scrap/withlifeentity.h"
#include "scrap/console.h"
#include "scrap/global.h"
#include "scrap/mathlib.h"
#include "scrap/scrap.h"
#include "scrap/world.h"

#define CNETVCONTROLLER_VFTABLE GAME_VFTABLE(cNetVController_vftable, 0x3b61b8)

cNetVController *__thiscall cNetVController_ctor(void *mem, cEntity *v) {
	cNetVController *self = (cNetVController *)cNetController_ctor(mem);

	self->vehicle = v;
	self->vftable = CNETVCONTROLLER_VFTABLE;
	self->hit = 0;
	self->force = 0;
	self->b60 = 0;
	self->i6c = 0;
	self->height = 0.0f;
	return self;
}

/* vf03: an active vehicle always sends its position */
byte __thiscall cNetVController_Changed(cNetVController *self, byte flags, cEntity *e) {
	if (self->vehicle->flag_4c) {
		byte f = cNetController_Changed((cNetController *)self, flags, e) | NETC_POS;
		self->flags = f;
		return f;
	}
	return cNetController_Changed((cNetController *)self, flags, e);
}

/* vf00 */
void __thiscall cNetVController_Reset(cNetVController *self) {
	cNetController_Reset((cNetController *)self);
}

#define NETV_KEY_ACTIVE 0x08
#define NETV_KEY_CLOUD_ON 0x10
#define NETV_KEY_CLOUD 0x20
#define NETV_KEY_EMI 0x40
#define NETV_KEY_FIRE 0x80
#define NETV_KEY_LOCK_ENTITY 0x2000
#define NETV_KEY_LOCK_POINT 0x4000
#define NETV_FIRE_SCALE 100.0f		/* the fire delay comes as a byte of 1/100 s */
#define NETV_FIRE_TIME 10000.0f
#define NETV_CLOUD_TIME 10.0f
#define NETV_EMI_TIME 10.0f
#define NETV_POINT_STEP 10.0f		/* the points come as shorts relative to the vehicle */
#define NETV_UAMMO_MASK 0x7fff
#define NETV_UPGRADES 0x8000		/* the upgrade bits and the upgrade ammo follow */
#define NETV_MAX_UPGRADE 16

#define NETV_VEH(self) ((cVehicleEntity *)(self)->vehicle)
#define NETV_LIFE(self) ((cWithLifeEntity *)(self)->vehicle)

static inline void NetV_Read(cAbstractFile *msg, void *dst, int n) {
	msg->vftable->Read(msg, dst, n);
}

/* vf01: a server reads the controls of the client (weapon, camera height, target); a client the ammo levels, the
 * lock, fire, cloud and EMI, the life, the weapon, the attack, the hits and the upgrades. Then the state of the
 * cNetController, and the hit flags when its flag bit 7 is set */
void __thiscall cNetVController_Read(cNetVController *self, void *m) {
	cAbstractFile *msg = m;
	char name[0x40];
	vec3_t q, clamped;
	uint32_t server = g_pApp->flags & APPF_SERVER;
	byte shot = 0, lockPoint = 0, shotSet = 0;
	int16_t sx = 0, sy = 0, sz = 0, lx = 0, ly = 0, lz = 0;

	NetV_Read(msg, &self->keys, 2);
	if (server) {
		byte w;
		uint16_t id;
		NetV_Read(msg, &w, 1);
		Controls_SetWeapon(NETV_VEH(self)->controls, w);
		NetV_Read(msg, &self->height, 4);
		NetV_Read(msg, &id, 2);
		crt_sprintf(NETV_VEH(self)->target, "net%d", id);
		goto base;
	}
	byte mask;
	NetV_Read(msg, &mask, 1);
	for (int i = 0; i < g_pWorld->weapons.count; i++) {
		int ammo = TABLE_AT(g_pWorld->weapons, i)->ammo;
		float cur = cTargetLock_GetAmmo(&NETV_VEH(self)->weapons, ammo);
		float min = TABLE_AT(g_pWorld->weapons, i)->minAmmo;
		if (mask & (1u << (i & 31))) {
			float hi = min + 1.0f;
			if (hi > cur) {
				cWeaponManager_SetAmmo(&NETV_VEH(self)->weapons, ammo, hi);
			}
		} else {
			float lo = min - 1.0f;
			if (cur >= lo) {
				cWeaponManager_SetAmmo(&NETV_VEH(self)->weapons, ammo, 1.0f > min ? 0.0f : lo);
			}
		}
	}
	if (self->keys & NETV_KEY_LOCK_ENTITY) {
		uint16_t id;
		NetV_Read(msg, &id, 2);
		crt_sprintf(name, "net%d", id);
		cWeaponManager_LockEntity(&NETV_VEH(self)->weapons, name);
	} else if (self->keys & NETV_KEY_LOCK_POINT) {
		NetV_Read(msg, &lx, 2);
		NetV_Read(msg, &ly, 2);
		NetV_Read(msg, &lz, 2);
		lockPoint = 1;
	} else {
		cWeaponManager_LockPoint(&NETV_VEH(self)->weapons, NULL);
	}
	if (self->keys & NETV_KEY_FIRE) {
		byte b;
		NetV_Read(msg, &b, 1);
		NETV_LIFE(self)->fireDelay = (float)b / NETV_FIRE_SCALE;
		NETV_LIFE(self)->fireRemain = b ? NETV_FIRE_TIME : 0.0f;
	} else {
		NETV_LIFE(self)->fireRemain = 0.0f;
	}
	self->vehicle->vftable->SetActive(self->vehicle, (self->keys & NETV_KEY_ACTIVE) != 0);
	if (self->keys & NETV_KEY_CLOUD) {
		cVehicleEntity_SetCloud(self->vehicle, NETV_CLOUD_TIME, (self->keys & NETV_KEY_CLOUD_ON) != 0);
	} else {
		cVehicleEntity_SetCloud(self->vehicle, 0.0f, 0);
	}
	NETV_VEH(self)->emi = (self->keys & NETV_KEY_EMI) ? NETV_EMI_TIME : 0.0f;
	uint16_t life;
	NetV_Read(msg, &life, 2);
	if (NETV_VEH(self)->wl.life > 0 && life == 0) {
		self->vehicle->vftable->SetActive(self->vehicle, 1);
	}
	NETV_VEH(self)->wl.life = life;
	byte w;
	NetV_Read(msg, &w, 1);
	int old = NETV_VEH(self)->weapons.curWeap;
	w = (byte)cWeaponManager_FixWeapon(w);
	if (old != w) {
		cPy_Function_AddParamS(&TABLE_AT(g_pWorld->weapons, old)->onEnd, self->vehicle->name);
		cPy_Function_Execute(&TABLE_AT(g_pWorld->weapons, old)->onEnd, 0);
		NETV_VEH(self)->weapons.curWeap = w;
		cPy_Function_AddParamS(&TABLE_AT(g_pWorld->weapons, w)->onStart, self->vehicle->name);
		cPy_Function_Execute(&TABLE_AT(g_pWorld->weapons, w)->onStart, 0);
	}
	byte fl, changed;
	NetV_Read(msg, &fl, 1);
	if ((byte)cVehicleAttack_NetFlags(&NETV_LIFE(self)->attack, fl, &changed)) {
		uint16_t id;
		NetV_Read(msg, &id, 2);
		if (changed) {
			crt_sprintf(name, "net%d", id);
			cVehicleAttack_ShotAt(&NETV_LIFE(self)->attack, name, 0);
		} else {
			cVehicleAttack_Expire(&NETV_LIFE(self)->attack);
		}
	} else {
		/* a shot at a point: it is set after the position of cNetController */
		shotSet = changed;
		shot = 1;
		NetV_Read(msg, &sx, 2);
		NetV_Read(msg, &sy, 2);
		NetV_Read(msg, &sz, 2);
	}
	byte hits;
	uint16_t hitId;
	NetV_Read(msg, &hits, 1);
	NetV_Read(msg, &hitId, 2);
	if (hits != NETV_LIFE(self)->attack.hitCount) {
		NETV_LIFE(self)->attack.hitCount = hits;
		crt_sprintf(name, "net%d", hitId);
		cVehicleAttack_HitBy(&NETV_LIFE(self)->attack, name);
	}
	uint32_t ua = 0, ups = 0;
	byte upAmmo = 0;
	NetV_Read(msg, &ua, 2);
	cWeaponManager_SetAmmo(&NETV_VEH(self)->weapons, TABLE_AT(g_pWorld->weapons, NETV_VEH(self)->weapons.curWeap)->ammo,
		(float)(int)(ua & NETV_UAMMO_MASK));
	if (ua & NETV_UPGRADES) {
		NetV_Read(msg, &ups, 2);
		NetV_Read(msg, &upAmmo, 1);
	}
	for (int i = 0; i < g_pWorld->weapons.count; i++) {
		cWeaponManager_SetUpgrade(&NETV_VEH(self)->weapons, i, 0);
		for (int j = 1; j <= NETV_MAX_UPGRADE; j++) {
			if ((uint16_t)ups & (1u << (j - 1))) {
				cWeaponManager_SetUpgrade(&NETV_VEH(self)->weapons, i, j);
			}
		}
	}
	cWeaponManager_SetUAmmo(&NETV_VEH(self)->weapons, NETV_VEH(self)->weapons.curWeap, (float)upAmmo);
base:
	cNetController_Read((cNetController *)self, msg);
	if (shot) {
		if (shotSet) {
			self->vehicle->vftable->GetPos(self->vehicle, &q);
			q.x = (float)sx * NETV_POINT_STEP + q.x;
			q.y = (float)sy * NETV_POINT_STEP + q.y;
			q.z = (float)sz * NETV_POINT_STEP + q.z;
			cVehicleAttack_ShotAtPoint(&NETV_LIFE(self)->attack, &q, 0);
		} else {
			cVehicleAttack_Expire(&NETV_LIFE(self)->attack);
		}
	}
	if (lockPoint) {
		self->vehicle->vftable->GetPos(self->vehicle, &q);
		q.x = (float)lx * NETV_POINT_STEP + q.x;
		q.y = (float)ly * NETV_POINT_STEP + q.y;
		q.z = (float)lz * NETV_POINT_STEP + q.z;
		q = *Grid_ClampPos(g_pWorld->grid, &clamped, &q);
		cWeaponManager_LockPoint(&NETV_VEH(self)->weapons, &q);
	}
	if ((signed char)self->flags < 0) {
		byte b;
		NetV_Read(msg, &b, 1);
		self->force = ((b ^ self->b60) & 0xf0) != 0;
		byte prev = self->b60;
		self->b60 = b;
		self->hit = ((b ^ prev) & 0x0f) != 0;
	}
}

#define NETV_KEY_FIRE1 0x0001
#define NETV_KEY_FIRE2 0x0002
#define NETV_KEY_CANFIRE 0x0004
#define NETV_KEY_BOOST 0x1000		/* no boost left */
#define NETV_KEY_FWD 0x0100
#define NETV_KEY_BACK 0x0200
#define NETV_KEY_LEFT 0x0400
#define NETV_KEY_RIGHT 0x0800
#define NETV_KEY_TOGGLE 0x8000		/* the fire toggle bit of Controls_FromNet */
#define NETV_NO_ID 0x200		/* the id of a name that is not "net<n>" */
#define NETV_AXIS 0.5f
#define NETV_MIN_FIRE_DELAY 0.01f
#define NETV_CAM_TARGET_RADIUS 25000.0f

static inline void NetV_Write(cAbstractFile *msg, const void *src, int n) {
	msg->vftable->Write(msg, src, n);
}

/* the id of a "net<n>" name */
static uint32_t NetV_NameId(const char *name) {
	if (!crt_strncmp(name, "net", 3)) {
		return (uint16_t)crt_atoi(name + 3);
	}
	return NETV_NO_ID;
}

/* the offset of p from the vehicle in steps of 10 (the position is asked again for every axis) */
static void NetV_WriteOffset(cNetVController *self, cAbstractFile *msg, const vec3_t *p) {
	vec3_t pos;
	uint32_t s;

	s = (uint16_t)(int)((p->x - self->vehicle->vftable->GetPos(self->vehicle, &pos)->x) / NETV_POINT_STEP);
	NetV_Write(msg, &s, 2);
	s = (uint16_t)(int)((p->y - self->vehicle->vftable->GetPos(self->vehicle, &pos)->y) / NETV_POINT_STEP);
	NetV_Write(msg, &s, 2);
	s = (uint16_t)(int)((p->z - self->vehicle->vftable->GetPos(self->vehicle, &pos)->z) / NETV_POINT_STEP);
	NetV_Write(msg, &s, 2);
}

/* vf02: the keys from the controls and the state of the vehicle; then the controls of a "net" vehicle (the client
 * side: weapon, camera height, the target of the camera), else the state Read takes on a client; then the state of
 * cNetController and the hit flags, whose counters step while hit and force are 2 */
void __thiscall cNetVController_Write(cNetVController *self, byte flags, cEntity *e, void *m, byte force) {
	cAbstractFile *msg = m;
	cVehicleEntity *v = NETV_VEH(self);
	cVehicleControls *c = (cVehicleControls *)v->controls;
	byte isNet = Net_IsNetEntity(self->vehicle);
	uint32_t u;
	byte b;

	self->keys = 0;
	if (c->acc > NETV_AXIS) {
		self->keys = NETV_KEY_FWD;
	}
	if (-NETV_AXIS > c->acc) {
		self->keys |= NETV_KEY_BACK;
	}
	if (c->str > NETV_AXIS) {
		self->keys |= NETV_KEY_RIGHT;
	}
	if (-NETV_AXIS > c->str) {
		self->keys |= NETV_KEY_LEFT;
	}
	if (c->fire) {
		self->keys |= NETV_KEY_FIRE1;
	}
	if (c->cm) {
		self->keys |= NETV_KEY_FIRE2;
	}
	if ((byte)cVehicleEntity_CanFire(v)) {
		self->keys |= NETV_KEY_CANFIRE;
	}
	v = NETV_VEH(self);
	if (v->wl.obj.base.flag_4c) {
		self->keys |= NETV_KEY_ACTIVE;
	}
	if (((cWithLifeEntity *)v)->fireRemain > 0.0f) {
		self->keys |= NETV_KEY_FIRE;
	}
	if (v->boost == 0.0f) {
		self->keys |= NETV_KEY_BOOST;
	}
	if (cTargetLock_GetEntity(&v->weapons)) {
		self->keys |= NETV_KEY_LOCK_ENTITY;
	}
	if (cTargetLock_GetPoint(&NETV_VEH(self)->weapons)) {
		self->keys |= NETV_KEY_LOCK_POINT;
	}
	v = NETV_VEH(self);
	if (((cVehicleControls *)v->controls)->netFire) {
		self->keys |= NETV_KEY_TOGGLE;
	}
	if (v->cloud > 0.0f) {
		self->keys |= NETV_KEY_CLOUD;
		if (v->cloudOn) {
			self->keys |= NETV_KEY_CLOUD_ON;
		}
	}
	if (((const cWithLifeEntityVtblFull *)v->wl.obj.base.vftable)->IsEMIed(&v->wl.obj.base)) {
		self->keys |= NETV_KEY_EMI;
	}
	u = self->keys;
	NetV_Write(msg, &u, 2);
	if (isNet) {
		b = (byte)Controls_GetWeapon(NETV_VEH(self)->controls);
		NetV_Write(msg, &b, 1);
		cFollowCamEntity *cam = (cFollowCamEntity *)NETV_VEH(self)->wl.obj.cam;
		if (cam) {
			self->height = (float)Game_Sin((double)-cam->camAng) * cam->dist;
		} else {
			self->height = 0.0f;
		}
		float h = self->height;
		NetV_Write(msg, &h, 4);
		if (!self->vehicle->flag_4c) {
			/* the original calls IsType on a NULL camera when the camera has no name */
			cEntity *cam = World_FindEntity(cWorld_GetCamName(g_pWorld, 0));
			if (cam->vftable->IsType(cam, "FCam")) {
				cEntity *t;
				if (cCamEntity_GetTarget(cam)) {
					t = cCamEntity_GetTarget(cam);
				} else {
					t = NULL;
					if (!cWorld_TestSphereEntities(g_pWorld, NULL, (void *)1, (const vec3_t *)((byte *)cam + 0x7c),
						    NETV_CAM_TARGET_RADIUS, &t, 1, 0) || !t) {
						goto target;
					}
				}
				Str_Copy(NETV_VEH(self)->target, t->name, sizeof(NETV_VEH(self)->target));
			}
		}
target:
		u = NetV_NameId(NETV_VEH(self)->target);
		NetV_Write(msg, &u, 2);
		goto base;
	}
	b = 0;
	for (int i = 0; i < g_pWorld->weapons.count; i++) {
		cWeapInfo *wi = TABLE_AT(g_pWorld->weapons, i);
		cWeapInfo *wm = TABLE_AT(g_pWorld->weapons, i);
		if (cTargetLock_GetAmmo(&NETV_VEH(self)->weapons, wi->ammo) > wm->minAmmo) {
			b = (byte)(b | (1u << (i & 31)));
		}
	}
	NetV_Write(msg, &b, 1);
	if (self->keys & NETV_KEY_LOCK_ENTITY) {
		u = NetV_NameId(cTargetLock_GetEntity(&NETV_VEH(self)->weapons));
		NetV_Write(msg, &u, 2);
	} else if (self->keys & NETV_KEY_LOCK_POINT) {
		NetV_WriteOffset(self, msg, cTargetLock_GetPoint(&NETV_VEH(self)->weapons));
	}
	if (self->keys & NETV_KEY_FIRE) {
		cWithLifeEntity *wl = NETV_LIFE(self);
		float fd = wl->fireDelay;
		if (NETV_MIN_FIRE_DELAY > fd || NETV_MIN_FIRE_DELAY > wl->fireRemain) {
			Con_Warnf("FireDelayTime is 0 or less in '%s'", self->vehicle->name);
			b = 0;
		} else {
			b = (byte)(int)(fd * NETV_FIRE_SCALE);
		}
		NetV_Write(msg, &b, 1);
	}
	v = NETV_VEH(self);
	u = 0.0f >= (float)v->wl.life ? 0 : (uint16_t)v->wl.life;
	NetV_Write(msg, &u, 2);
	byte wf = (byte)NETV_VEH(self)->weapons.curWeap;
	b = wf;
	NetV_Write(msg, &b, 1);
	cVehicleAttack_NetChanged(&NETV_LIFE(self)->attack, &wf);
	b = wf;
	NetV_Write(msg, &b, 1);
	const char *name = cHitInfo_GetName(&NETV_LIFE(self)->attack);
	if (*name) {
		u = NetV_NameId(name);
		NetV_Write(msg, &u, 2);
	} else {
		vec3_t p = *cVehicleAttack_GetPoint(&NETV_LIFE(self)->attack);
		NetV_WriteOffset(self, msg, &p);
	}
	cWithLifeEntity *wl = NETV_LIFE(self);
	b = wl->attack.hitCount;
	const char *by = Nto1Node_TargetName(&wl->attack.hitBy);
	if (by && *Nto1Node_TargetName(&NETV_LIFE(self)->attack.hitBy)) {
		u = (uint16_t)crt_atoi(Nto1Node_TargetName(&NETV_LIFE(self)->attack.hitBy) + 3);
	} else {
		u = (uint16_t)crt_atoi(self->vehicle->name + 3);
	}
	NetV_Write(msg, &b, 1);
	NetV_Write(msg, &u, 2);
	v = NETV_VEH(self);
	uint32_t ammo = (uint16_t)(int)cTargetLock_GetAmmo(&v->weapons, TABLE_AT(g_pWorld->weapons, v->weapons.curWeap)->ammo);
	uint32_t ups = 0;
	for (int i = 0; i < g_pWorld->weapons.count; i++) {
		int up = cWeaponManager_GetUpgrade(&NETV_VEH(self)->weapons, i);
		if (up > 0) {
			ups = (uint16_t)(ups | (1u << ((up - 1) & 31)));
		}
	}
	if ((uint16_t)ups) {
		u = (uint16_t)(ammo | NETV_UPGRADES);
		NetV_Write(msg, &u, 2);
		u = (uint16_t)ups;
		NetV_Write(msg, &u, 2);
		v = NETV_VEH(self);
		b = (byte)(int)cWeaponManager_GetUAmmo(&v->weapons, v->weapons.curWeap);
		NetV_Write(msg, &b, 1);
	} else {
		u = ammo & NETV_UAMMO_MASK;
		NetV_Write(msg, &u, 2);
	}
base:
	if (self->i6c > 0) {
		flags &= NETC_FULL | NETC_POS | NETC_VEL | 0x80;
		self->i6c--;
	}
	cNetController_Write((cNetController *)self, flags, e, msg, force);
	if ((signed char)self->flags < 0) {
		int hit = self->hit;
		if (hit == 2) {
			self->b60 = (byte)((self->b60 & 0xf0) | ((self->b60 + 1) & 0x0f));
		}
		if (self->force == 2) {
			self->b60 = (byte)(((self->b60 & 0xf0) + 0x10) & 0xf0) | (self->b60 & 0x0f);
		}
		self->hit = hit - 1 < 0 ? 0 : hit - 1;
		self->force = self->force - 1 < 0 ? 0 : self->force - 1;
		b = self->b60;
		NetV_Write(msg, &b, 1);
	}
}
