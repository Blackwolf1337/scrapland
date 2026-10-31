/*
 * Entities/Vehicle/Weaps/Tesla.cpp of the game: the Tesla, a ray of every mounted bay to the target that hurts and
 * pushes it, see include/scrap/weaps2.h. While it fires without a hit, the aim cone of the ship narrows from
 * TeslaMaxAngRange to TeslaMinAngRange (Veh_TeslaAng..Veh_TeslaMinAng at the speed Veh_TeslaAngSpeed); a hit widens
 * it again. The shot handler Tesla_Shoot is in Swarm.c (it sits in the Swarm code).
 */
#include "scrap/vehicleentity.h"
#include "scrap/weaps2.h"

#define TESLA_UPG_TRACK 5		/* the upgrade that pushes a ship against its flight */
#define TESLA_UPG_WIDE 6		/* the upgrade that keeps the HUD item a quarter open */
#define TESLA_CONE 0x1.80406ep-1f	/* the cone of the aim help */

/* ship name, hit point x y z (unused), target name: the ray effect (once per shot) and the push of the target; a
 * hit opens the aim cone again */
void Tesla_CreateNative(cAbstractFile *f) {
	char name[0x80], targetName[0x80];
	vec3_t hit;

	cAbstractFile_ReadString(f, name, 0x80);
	Weap_ReadVec(f, &hit);
	cAbstractFile_ReadString(f, targetName, 0x80);
	cEntity *v = Weap_Vehicle(name);
	cTargetLock *wm = Veh_Weapons(v);
	byte shooting = wm->flag46;
	cWeaponManager_SetFlag46(wm, 1);
	int n = 0;
	if (Weap_Vtbl(v)->GetModel(v)) {
		n = cWeaponManager_NumSlots(wm, g_TeslaIDWeap, 1);
		if (n > 0 && !shooting) {
			cDebrisSys *sys = cWorld_GetDebrisSys(g_pWorld, g_TeslaShootFile);
			cDebris *d = cDebrisSys_NewElement(sys);
			d->speed = 0.0f;
			cDebrisSys_Drop(sys, d);
			cObjElementFXTesla *fx = Debris_Element(d);
			if (fx) {
				if (!fx->on) {
					fx->on = 1;
				}
				cObjElementFXTesla_SetEmitter(fx, v);
			}
		}
	}
	cEntity *w = World_FindEntity(targetName);
	if (!w || !Entity_IsType(w, TYEN_WITH_LIFE)) {
		return;
	}
	Tesla_Push(v, w, n);
	float down;
	if (g_VehicleCenterPadLock != 0.0f && cVehicleEntity_GetOwnerSlot(v) != -1) {
		down = 1.0f / g_TeslaDownRangeTimePad;
	} else {
		down = 1.0f / g_TeslaDownRangeTime;
	}
	float up = 1.0f / g_TeslaUpRangeTime;
	cAutoAim_SetAngle(Veh_Aim(v), (Veh_TeslaAng(v) - Veh_TeslaMinAng(v)) * 0.05f * (up + down) + Veh_AimLimit(v));
	cWithLifeEntity_SetSndTime(w, 0.5f, (n - 1) / 2);
}

/* ship name, player, item name: the HUD item of the Tesla grows and turns with the open part of the cone, else it
 * turns back */
void Tesla_HudNative(cAbstractFile *f) {
	char name[0x80], itemName[0x80];
	int player;

	cAbstractFile_ReadString(f, name, 0x80);
	f->vftable->Read(f, &player, 4);
	cAbstractFile_ReadString(f, itemName, 0x80);
	cEntity *v = Weap_Vehicle(name);
	cScorerItem *item = cScorer_FindItem(Scorer_Get(player), itemName);
	if (!item) {
		return;
	}
	float k = 0.0f;
	int level = cWeaponManager_GetUpgrade(Veh_Weapons(v), g_TeslaIDWeap);
	if (Veh_TeslaAngSpeed(v) != 0.0f) {
		float min = Veh_TeslaMinAng(v);
		k = (Veh_AimLimit(v) - min) / (Veh_TeslaAng(v) - min);
	}
	if (Vehicle_Controls(v)->fire && level == TESLA_UPG_WIDE) {
		k = Float_MaxSS(0.25f, k);
	}
	float size = k * 4.0f + 20.0f;
	Item_SizeX(item) = size;
	Item_SizeY(item) = size;
	int b = (int)g_CannonScorerColorBlue - (int)(k * -255.0f);
	uint32_t a = 0xffffffbfu - (uint32_t)(int)(k * -64.0f);
	if (b > 0xff) {
		b = 0xff;
	}
	Item_Color(item) = (((a << 8 | g_CannonScorerColorRed) << 8 | g_CannonScorerColorGreen) << 8) | (uint32_t)b;
	float delta = Scorer_Delta(Scorer_Get(player));
	if (k == 0.0f) {
		Weap_HudSpinDown(item, delta * 0x1.921fb6p+3f);
	} else {
		Item_Rot(item) = Item_Rot(item) - k * 0x1.f6a7a2p+3f * delta;
	}
}

/* ship name: the shot (a client only aims, unless the ship hit something), one life per mounted bay */
void Tesla_FireNative(cAbstractFile *f) {
	char name[0x80];
	vec3_t ang, out;
	cEntity *target;
	float dist;

	cAbstractFile_ReadString(f, name, 0x80);
	cEntity *v = Weap_Vehicle(name);
	byte shot = Tesla_Shoot(v);
	ang.x = Veh_AimAng(v);
	ang.y = Veh_AimAngY(v);
	ang.z = 0.0f;
	target = NULL;
	dist = g_TeslaRange;
	if (g_pApp->flags & APPF_CLIENT) {
		if (!shot || *cHitInfo_GetName(Veh_HitInfo(v))) {
			return;
		}
		Weap_Vtbl(v)->GetTargetAngle(v, &ang.x, dist, &target);
		Weap_Vtbl(v)->AimWeapon(v, ang.x, ang.y, &dist, &out, &target, 0);
		cVehicleAttack_ShotAtPoint(Veh_HitInfo(v), &out, 0);
		return;
	}
	cTargetLock *wm = Veh_Weapons(v);
	int n = cWeaponManager_NumSlots(wm, g_TeslaIDWeap, 1);
	if (!n) {
		return;
	}
	Weap_Vtbl(v)->GetTargetAngle(v, &ang.x, dist, &target);
	Weap_Vtbl(v)->AimWeapon(v, ang.x, ang.y, &dist, &out, &target, 8);
	byte isCar = target && Entity_IsType(target, TYEN_VEHICLE);
	byte isLife = target && Entity_IsType(target, TYEN_WITH_LIFE);
	if (isCar) {
		cVehicleAttack_ShotAt(Veh_HitInfo(v), target->name, 1);
	} else {
		cVehicleAttack_ShotAtPoint(Veh_HitInfo(v), &out, 1);
	}
	if (isLife) {
		if (!Ent_God(target)) {
			Ent_Life(target) -= n;
		}
		if (!isCar) {
			Tesla_Push(v, target, n);
		}
	}
	/* the upgrades 5 and 6 use up their own ammo, at 0 the script restores the weapon */
	int level = cWeaponManager_GetUpgrade(wm, g_TeslaIDWeap);
	if ((uint32_t)(level - TESLA_UPG_TRACK) <= 1) {
		float u = Weap_X87Sub(cWeaponManager_GetUAmmo(wm, g_TeslaIDWeap), (float)n * 0.5f);
		if (0.0f >= u) {
			cWeaponManager_SetUpgrade(wm, g_TeslaIDWeap, 0);
			u = 0.0f;
			cPy_Function_AddParamI(g_RestoreUpgradeFunc, level);
			cPy_Function_Execute(g_RestoreUpgradeFunc, 0);
		}
		cWeaponManager_SetUAmmo(wm, g_TeslaIDWeap, u);
	}
	float rest = Weap_X87Sub(cTargetLock_GetAmmo(wm, g_TeslaIDAmmo), (float)n * g_TeslaAmmoCost);
	cWeaponManager_SetAmmo(wm, g_TeslaIDAmmo, Float_MaxSS(0.0f, rest));
}

/* the push of n bays on e; the upgrade 5 pushes a ship against its flight instead */
void Tesla_Push(cEntity *v, cEntity *e, int n) {
	vec3_t tmp, nd;

	if (!e || !Entity_IsType(e, TYEN_WITH_LIFE)) {
		return;
	}
	int level = cWeaponManager_GetUpgrade(Veh_Weapons(v), g_TeslaIDWeap);
	if (Entity_IsType(e, TYEN_WALKCHAR)) {
		return;
	}
	if (level == TESLA_UPG_TRACK && Entity_IsType(e, TYEN_VEHICLE)) {
		const vec3_t *dir = Vec3_NormalizeTo(&nd, Weap_Vtbl(e)->GetVel(e, &tmp));
		float x = -dir->x;
		float y = -dir->y;
		float z = -dir->z;
		float k = (float)n * g_TeslaTrackForce;
		Veh_PushTime(e) = 0.3f;
		Veh_Push(e).x = x * k;
		Veh_Push(e).y = y * k;
		Veh_Push(e).z = z * k;
		return;
	}
	Weap_PushAway(v, e, n, g_TeslaHitForce);
}

void Tesla_AimOnNative(cAbstractFile *f) {
	char name[0x80];

	cAbstractFile_ReadString(f, name, 0x80);
	cEntity *v = Weap_Vehicle(name);
	Tesla_AimOn(v);
	cWeaponManager_SetFlag46(Veh_Weapons(v), 0);
}

void Tesla_AimOffNative(cAbstractFile *f) {
	char name[0x80];

	cAbstractFile_ReadString(f, name, 0x80);
	cEntity *v = Weap_Vehicle(name);
	Veh_AimMode(v) = 0;
	Veh_TeslaAngSpeed(v) = 0.0f;
	cWeaponManager_SetFlag46(Veh_Weapons(v), 0);
}

/* the aim help of the Tesla: the cone opens wide (wider with a pad and VehicleCenterPadLock) */
void Tesla_AimOn(cEntity *v) {
	Veh_AimMode(v) = 2;
	Veh_AimCone(v) = TESLA_CONE;
	float min = (float)(((double)g_TeslaMinAngRange * WEAP_PI) / 180.0);
	Veh_AimDist(v) = g_TeslaDistLimit;
	float max = g_TeslaMaxAngRange;
	if (g_VehicleCenterPadLock != 0.0f && cVehicleEntity_GetOwnerSlot(v) != -1) {
		max = g_TeslaMaxAngRangePad;
	}
	Veh_TeslaAngSpeed(v) = 0.0f;
	Veh_TeslaAng(v) = (float)(((double)max * WEAP_PI) / 180.0);
	Veh_TeslaMinAng(v) = min;
	cAutoAim_SetAngle(Veh_Aim(v), min);
}
