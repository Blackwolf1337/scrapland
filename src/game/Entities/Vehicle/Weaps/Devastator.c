/*
 * Entities/Vehicle/Weaps/Devastator.cpp of the game: the native functions of the Devastator (a spread shot that
 * explodes at the hit, "!DevastatorXplosionCall") and of the EMI, see include/scrap/weapons.h. Also the native
 * "!InfernoClientFire" that hands the name to Weaps/Inferno.cpp.
 * Not here: DevastatorCreate (rva 0x11e250, the shot effects, hits and sounds of a shot).
 */
#include "scrap/ai.h"
#include "scrap/fxmisc.h"
#include "scrap/mathlib.h"
#include "scrap/net.h"
#include "scrap/scorer.h"
#include "scrap/weapons.h"

#define DEV_PI 3.14159274f		/* rva 0x3a3a30 */

/* the shot sounds 0xd..0xf of the vehicle loop with the recharge time of one slot */
void Devastator_Start(cEntity *v) {
	if (!(cTargetLock_GetAmmo(Vehicle_WeapMgr(v), g_DevastatorIDAmmo) > 0.0f)) {
		return;
	}
	Devastator_SetUp(v);
	int n = cWeaponManager_NumSlots(Vehicle_WeapMgr(v), g_DevastatorIDWeap, 1);
	if (!n) {
		return;
	}
	if (Vehicle_WeapMgr(v)->weapSnd == -1) {
		cWeaponManager_SetWeapSnd(Vehicle_WeapMgr(v), 0xd);
		cSndItem *it = PtrArray_Get(Vehicle_Sounds(v), 0xd);
		if (it) {
			it->time = 0.0f;
		}
	}
	float t = g_DevastatorRechargeTime / (float)n;
	cWeaponManager_DelayFire(Vehicle_WeapMgr(v), t);
	for (int i = 0xd; i <= 0xf; i++) {
		cSndItem *it = PtrArray_Get(Vehicle_Sounds(v), i);
		if (it) {
			it->times[0] = t;
		}
	}
}

/* "!DevastatorFX" (vehicle, player, item): the HUD item turns while the vehicle fires (the upgrade 8 keeps turning),
 * the upgrade 7 pulses */
void Devastator_FX(cAbstractFile *params) {
	char name[128], itemName[128];
	int player;

	cAbstractFile_ReadString(params, name, 0x80);
	params->vftable->Read(params, &player, 4);
	cAbstractFile_ReadString(params, itemName, 0x80);
	cEntity *v = Weap_GetVehicle(name);
	cScorerItem *item = cScorer_FindItem(Scorer_Get(player), itemName);
	if (!item) {
		return;
	}
	float dt = Scorer_Delta(Scorer_Get(player));
	int up = cWeaponManager_GetUpgrade(Vehicle_WeapMgr(v), g_DevastatorIDWeap);
	if (Controls_Fire(Vehicle_Controls(v)) && up == 8) {
		ScorerItem_Angle(item) = dt * DEV_PI + ScorerItem_Angle(item);
	} else {
		float s = dt * DEV_PI;
		float a = NormAng(ScorerItem_Angle(item));
		ScorerItem_Angle(item) = a;
		if (0.0f > a + s) {
			ScorerItem_Angle(item) = a + s;
		} else if (a - s > 0.0f) {
			ScorerItem_Angle(item) = a - s;
		} else {
			ScorerItem_Angle(item) = 0.0f;
		}
	}
	if (Controls_Fire(Vehicle_Controls(v)) && up == 7) {
		float time = Scorer_Time(Scorer_Get(player));
		int k = (int)((double)time * 1024.0);
		int m = k % 0x1fe - 0xff;
		int ab = m < 0 ? -m : m;
		ScorerItem_Color(item) = (((uint32_t)(ab / 4 - 0x41) << 8 | g_CannonScorerColorRed) << 8
			| g_CannonScorerColorGreen) << 8 | g_CannonScorerColorBlue;
		float size = (float)ab * 0.03125f + 20.0f;
		ScorerItem_SizeX(item) = size;
		ScorerItem_SizeY(item) = size;
		return;
	}
	ScorerItem_SizeX(item) = 20.0f;
	ScorerItem_SizeY(item) = 20.0f;
	ScorerItem_Color(item) = Weap_CannonColor();
}

/* "!DevastatorFire" (vehicle): a shot spread at random around the aim, one life of the entity hit */
void Devastator_Fire(cAbstractFile *params) {
	char name[128];
	float ang[3];
	vec3_t hit, a, b;
	cEntity *target;
	float dist;

	cAbstractFile_ReadString(params, name, 0x80);
	cEntity *v = Weap_GetVehicle(name);
	Devastator_Start(v);
	if (g_pApp->flags & APPF_CLIENT) {
		return;
	}
	cWeaponManager *wm = Vehicle_WeapMgr(v);
	float spread = (float)((double)g_DevastatorSpreadAngle * 3.141592653589793 / 180.0);
	if (cWeaponManager_GetUpgrade(wm, g_DevastatorIDWeap) == 8) {
		spread = spread * 0.75f;
	}
	float lo = -spread;
	float range = spread - lo;
	double dlo = lo;
	float rx = (float)(Weap_Rand01() * (double)range + dlo);
	float ry = (float)(Weap_Rand01() * (double)range + dlo);
	ang[2] = 0.0f;
	ang[0] = Vehicle_AimAng(v)[0] + ry;
	ang[1] = Vehicle_AimAng(v)[1] + rx;
	Controls_RumbleA(Vehicle_Controls(v), 0.7f, 0.2f);
	Controls_RumbleB(Vehicle_Controls(v), 0.35f, 0.2f);
	dist = g_DevastatorRange;
	target = Vehicle_AimTarget(v);
	if (target) {
		/* the shot ends near the target of the aim help */
		const vec3_t *pa = v->vftable->GetPos(v, &a);
		const vec3_t *pb = target->vftable->GetPos(target, &b);
		float dx = pb->x - pa->x;
		float dy = pb->y - pa->y;
		float dz = pb->z - pa->z;
		float d = (float)__builtin_sqrt((double)(dy * dy + dx * dx + dz * dz));
		if (dist > d) {
			float r = g_DevastatorExplosionRadius;
			dist = (float)(Weap_Rand01() * (double)r + 0.0) + d;
		}
	}
	v->vftable->GetTargetAngle(v, ang, dist, &target);
	v->vftable->AimWeapon(v, ang[0], ang[1], &dist, &hit, &target, 8);
	if (target) {
		if (Entity_IsType(target, TYEN_WITH_LIFE) && !Entity_God(target)) {
			Entity_LifeI(target)--;
		}
		if (target && Entity_IsType(target, TYEN_VEHICLE)) {
			cVehicleAttack_ShotAt(Vehicle_Attack(v), target->name, 1);
			goto ammo;
		}
	}
	cVehicleAttack_ShotAtPoint(Vehicle_Attack(v), &hit, 1);
ammo:;
	float rest = Weap_X87Sub(cTargetLock_GetAmmo(wm, g_DevastatorIDAmmo), g_DevastatorAmmoCost);
	cWeaponManager_SetAmmo(wm, g_DevastatorIDAmmo, Float_MaxSS(0.0f, rest));
	/* the upgrades 7 and 8 use up their own ammo, at 0 the script restores the weapon */
	int up = cWeaponManager_GetUpgrade(wm, g_DevastatorIDWeap);
	if ((uint32_t)(up - 7) > 1) {
		return;
	}
	float u = Weap_X87Sub(cWeaponManager_GetUAmmo(wm, g_DevastatorIDWeap), 2.0f);
	if (0.0f >= u) {
		cWeaponManager_SetUpgrade(wm, g_DevastatorIDWeap, 0);
		u = 0.0f;
		cPy_Function_AddParamI(g_RestoreUpgradeFunc, up);
		cPy_Function_Execute(g_RestoreUpgradeFunc, 0);
	}
	cWeaponManager_SetUAmmo(wm, g_DevastatorIDWeap, u);
}

/* "!DevastatorPress" (vehicle) */
void Devastator_Press(cAbstractFile *params) {
	char name[128];

	cAbstractFile_ReadString(params, name, 0x80);
	cEntity *v = Weap_GetVehicle(name);
	Devastator_SetUp(v);
}

/* "!DevastatorRelease" (vehicle) */
void Devastator_Release(cAbstractFile *params) {
	char name[128];

	cAbstractFile_ReadString(params, name, 0x80);
	cEntity *v = Weap_GetVehicle(name);
	Vehicle_AimMode(v) = 0;
}

/* the aim help of the Devastator, wider for a pad with VehicleCenterPadLock */
void Devastator_SetUp(cEntity *v) {
	Vehicle_AimMode(v) = 1;
	float ang;
	if (g_VehicleCenterPadLock != 0.0f && cVehicleEntity_GetOwnerSlot(v) != -1) {
		ang = 0.34906584f;
	} else {
		ang = 0.2443461f;
	}
	Vehicle_AimF6f4(v) = 0.75049156f;
	cAutoAim_SetAngle(Vehicle_AutoAim(v), ang);
	Vehicle_AimRange(v) = g_DevastatorDistLimit;
	cAutoAim_Update(Vehicle_AutoAim(v), v, 0.025f);
}

/* "!DevastatorXplosionCall" (entity): the damage and push of the explosion at g_DevCenter */
void Devastator_XplosionCall(cAbstractFile *params) {
	vec3_t p, in, r, n;

	cAbstractFile_ReadString(params, g_DevVictim, 0x80);
	cEntity *victim = World_FindEntity(g_DevVictim);
	if (!victim || !Entity_IsType(victim, TYEN_WITH_LIFE)) {
		return;
	}
	int oldLife = Entity_LifeI(victim);
	const vec3_t *pp = victim->vftable->GetPos(victim, &p);
	float dy = pp->y - g_DevCenter->y;
	float dx = pp->x - g_DevCenter->x;
	float dz = pp->z - g_DevCenter->z;
	float d = (float)__builtin_sqrt((double)(dy * dy + dx * dx + dz * dz));
	if (d != 0.0f) {
		float inv = 1.0f / d;
		dy = dy * inv;
		dx = dx * inv;
		dz = dz * inv;
	}
	if (1.0f > d) {
		/* at the center: a random direction */
		double r1 = Weap_Rand01();
		double r2 = Weap_Rand01();
		double r3 = Weap_Rand01();
		in.x = (float)((r3 + r3) - 1.0);
		in.y = (float)((r2 + r2) - 1.0);
		in.z = (float)((r1 + r1) - 1.0);
		const vec3_t *dir = Vec3_NormalizeTo(&r, &in);
		dx = dir->x;
		dy = dir->y;
		dz = dir->z;
		d = 1.0f;
	}
	float rad = g_DevastatorExplosionRadius * 1.5f;
	float f = (rad - Float_MaxSS(1.0f, d - Entity_Radius(victim))) / rad;
	int dmg = (int)(g_DevastatorExtraDamage * f + g_DevastatorMinDamage);
	if (World_IsEasySingle()) {
		cEntity *shooter = World_FindEntity(g_DevShooter);
		if (shooter && shooter->vftable->GetAI(shooter)) {
			dmg = dmg / 2;
			if (dmg <= 0) {
				dmg = 1;
			}
		}
	}
	if (!Entity_God(victim)) {
		Entity_LifeI(victim) -= dmg;
	}
	if (oldLife > Entity_LifeI(victim)) {
		cVehicleAttack_HitBy(Vehicle_Attack(victim), g_DevShooter);
	}
	if (!Entity_IsType(victim, TYEN_WALKCHAR)) {
		float x = dx * f;
		float y = dy * f;
		float z = dz * f;
		float k = g_DevastatorHitForce * 1000.0f / Entity_Mass(victim);
		x = x * k;
		y = y * k;
		z = z * k;
		const vec3_t *vel = WEnt_Vtbl(victim)->GetVel(victim, &r);
		n.x = x + vel->x;
		n.y = vel->y + y;
		n.z = vel->z + z;
		Entity_SetVel(victim, &n);
	}
	if (Entity_IsType(victim, TYEN_VEHICLE) && Vehicle_Hit(victim)) {
		*(int *)(Vehicle_Hit(victim) + 0x64) = 2;
	}
}

/* "!EMIFire" (vehicle): the script makes the EMI with the ammo of the vehicle */
void EMI_Fire(cAbstractFile *params) {
	char name[128], emiName[128];
	vec3_t pos, ang;

	cAbstractFile_ReadString(params, name, 0x80);
	cEntity *v = World_FindEntity(name);
	if (!v || !Entity_IsType(v, TYEN_VEHICLE)) {
		return;
	}
	cWeaponManager *wm = Vehicle_WeapMgr(v);
	float fammo = cTargetLock_GetAmmo(wm, g_EMIIDAmmo);
	int ammo = (int)fammo;
	if (Vehicle_IsEMIed(v) || ammo < 1) {
		return;
	}
	Vehicle_WeapMgr(v)->cmStamp = -g_EMIRechargeTime;
	cSndItem *it = PtrArray_Get(Vehicle_Sounds(v), 0x13);
	if (it) {
		cSndItem *src = PtrArray_Get(Vehicle_Sounds(v), 9);
		if (src) {
			it->volume = src->vols[0];
			it->pitch = src->pitches[0];
			cSndItem_Play(it, v);
		}
	}
	if (g_pApp->flags & APPF_CLIENT) {
		return;
	}
	Net_GetObjName(emiName);
	v->vftable->GetPos(v, &pos);
	Entity_GetAng(v, &ang);
	cPy_Function_AddParamS(g_CreateEMIFunc, emiName);
	cPy_Function_AddParamF(g_CreateEMIFunc, pos.x);
	cPy_Function_AddParamF(g_CreateEMIFunc, pos.y);
	cPy_Function_AddParamF(g_CreateEMIFunc, pos.z);
	cPy_Function_AddParamF(g_CreateEMIFunc, ang.x);
	cPy_Function_AddParamF(g_CreateEMIFunc, ang.y);
	cPy_Function_AddParamF(g_CreateEMIFunc, ang.z);
	cPy_Function_Execute(g_CreateEMIFunc, 0);
	cEntity *emi = World_FindEntity(emiName);
	if (!emi) {
		return;
	}
	Controls_RumbleA(Vehicle_Controls(v), 0.4f, 0.2f);
	cMissileEntity_SetF3f4(emi, 0.0f);
	cMissileEntity_SetF3f8(emi, 0.0f);
	ammo = (int)fammo;
	cMissileEntity_SetAmmo(emi, g_EMIAmmoNeed <= ammo ? g_EMIAmmoNeed : ammo);
	cPy_Function_AddParamS(g_SetUpEMIFunc, emi->name);
	cPy_Function_Execute(g_SetUpEMIFunc, 0);
	cMissileEntity_SetLifeTime(emi, g_EMILifeTime - 0.2f);
	cPy_Function_SetFunc(Missile_OnHit(emi), "!EMIOnHit");
	cWeaponManager_SetAmmo(wm, g_EMIIDAmmo, (float)(ammo - Missile_Ammo(emi)));
}

/* "!EMIOnHit" (EMI, ?, entity): a vehicle near the EMI is out for 10 seconds, further away for a random time (or not
 * at all); a missile in the radius loses its target */
void EMI_OnHit(cAbstractFile *params) {
	char emiName[128], other[128], name[128];
	vec3_t a, b;

	cAbstractFile_ReadString(params, emiName, 0x80);
	cAbstractFile_ReadString(params, other, 0x80);
	cAbstractFile_ReadString(params, name, 0x80);
	cEntity *emi = World_FindEntity(emiName);
	cEntity *e = World_FindEntity(name);
	if (!emi || !e) {
		return;
	}
	const vec3_t *pa = e->vftable->GetPos(e, &a);
	const vec3_t *pb = emi->vftable->GetPos(emi, &b);
	float dx = pb->x - pa->x;
	float dy = pb->y - pa->y;
	float dz = pb->z - pa->z;
	float d = (float)__builtin_sqrt((double)(dy * dy + dx * dx + dz * dz));
	float r = Missile_Radius(emi);
	if (Entity_IsType(e, TYEN_VEHICLE)) {
		if (g_EMILowRadiusFactor * r >= d) {
			Vehicle_EMITime(e) = 10.0f;
			return;
		}
		float t = Vehicle_EMITime(e);
		if (1.0f > t) {
			if (crt_rand() & 1) {
				Vehicle_EMITime(e) = 0.0f;
				return;
			}
			Vehicle_EMITime(e) = Rand_FloatRange(0.0f, d / r);
			return;
		}
		if (t > 5.0f) {
			Vehicle_EMITime(e) = 5.0f;
		}
		return;
	}
	if (Entity_IsType(e, "Missile") && r >= d) {
		cMissileEntity_SetMode(e, 1);
		cMissileEntity_SetHitName(e, (const char *)Game_Ptr(0x3a3228));
	}
}

/* "!InfernoClientFire" (vehicle) */
void Inferno_ClientFireNet(cAbstractFile *params) {
	char name[128];

	cAbstractFile_ReadString(params, name, 0x80);
	Inferno_ClientFire(name);
}
