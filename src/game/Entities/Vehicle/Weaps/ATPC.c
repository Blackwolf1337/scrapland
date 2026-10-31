/*
 * Entities/Vehicle/Weaps/ATPC.cpp of the game: the native functions of the ATPC weapon scripts and of the debris
 * cloud, see include/scrap/weapons.h. The shot costs ammo per mounted slot, hits along a line from the vehicle
 * ("!ATPCXplosionCall" on every entity of it) and pushes and damages the entities near the hit; "!ATPCCreate" makes
 * the shot effect and the sounds.
 */
#include "scrap/ai.h"
#include "scrap/fxmisc.h"
#include "scrap/net.h"
#include "scrap/scorer.h"
#include "scrap/weapons.h"

#include <string.h>

#define ATPC_PI 3.14159274f		/* rva 0x3a3a30 */

/* the recharge of the next shot (the upgrade 9 is faster) and the start sound of the upgrade */
void ATPC_SetRecharge(cEntity *v) {
	int up = cWeaponManager_GetUpgrade(Vehicle_WeapMgr(v), g_ATPCIDWeap);
	float ammo = cTargetLock_GetAmmo(Vehicle_WeapMgr(v), g_ATPCIDAmmo);
	int snd;
	if (ammo > 0.0f) {
		if (up == 9) {
			Vehicle_WeapMgr(v)->fireStamp = -g_ATPCUpgradeDelay;
			Vehicle_StartSnd(v, 0x11);
			return;
		}
		Vehicle_WeapMgr(v)->fireStamp = -g_ATPCDelay;
	}
	if (up == 9) {
		snd = 0x11;
	} else if (up == 10) {
		snd = 0x12;
	} else {
		snd = 0x10;
	}
	Vehicle_StartSnd(v, snd);
}

/* the sound at pos with the attenuation and volume of a sound variable */
void ATPC_PlaySound(const char *name, const vec3_t *pos, float attIni, float attEnd, float vol) {
	cSoundDesc desc;

	cSoundDesc_Init(&desc);
	desc.param1 = attIni;
	desc.param2 = attEnd;
	desc.param3 = 0.0f;
	cSoundDesc_SetPos(&desc, pos);
	desc.volume += Snd_Volume(vol);
	g_pSound->vftable->PlaySound(g_pSound, name, &desc, NULL);
}

/* "!ATPCCreate" (vehicle, hit point, target): the shot effect from the mounted slots (beyond the convergence distance
 * from their middle) to the hit, the hit sound and the capsule sound and shake for the player near the shot */
void ATPC_Create(cAbstractFile *params) {
	vec3_t hit, a, p, avg, in, rot, out, origin, mid, c, dir, near;
	matrix_t mat;

	cAbstractFile_ReadString(params, g_ATPCShooter, 0x80);
	params->vftable->Read(params, &hit.x, 4);
	params->vftable->Read(params, &hit.y, 4);
	params->vftable->Read(params, &hit.z, 4);
	cAbstractFile_ReadString(params, g_ATPCVictim, 0x80);
	cEntity *v = Weap_GetVehicle(g_ATPCShooter);
	cWeaponManager *wm = Vehicle_WeapMgr(v);
	const vec3_t *pa = v->vftable->GetPos(v, &a);
	float dy = hit.y - pa->y;
	float dx = hit.x - pa->x;
	float dz = hit.z - pa->z;
	float d = (float)__builtin_sqrt((double)(dy * dy + dx * dx + dz * dz));
	if (d > g_ATPCConvDist) {
		/* the middle of the mounted slots (the original starts from stack garbage without one) */
		byte first = 1;
		avg.x = avg.y = avg.z = 0.0f;
		for (int i = 0; i < 4; i++) {
			if (!cTargetLock_HasSlot(wm, g_ATPCIDWeap, i)) {
				continue;
			}
			int node = wm->nodes[g_ATPCIDAmmo][i];
			const matrix_t *m = CGameModel_NodeMatrix(WEnt_Vtbl(v)->GetObjData(v), &mat, node, 0, NULL, 0);
			p.x = m->m[12];
			p.y = m->m[13];
			p.z = m->m[14];
			if (first) {
				avg = p;
			} else {
				avg.z = (p.z - avg.z) * 0.5f + avg.z;
				avg.x = (p.x - avg.x) * 0.5f + avg.x;
				avg.y = (p.y - avg.y) * 0.5f + avg.y;
			}
			first = 0;
		}
		const vec3_t *pb = v->vftable->GetPos(v, &a);
		dy = hit.y - pb->y;
		dx = hit.x - pb->x;
		dz = hit.z - pb->z;
		float d2 = (float)__builtin_sqrt((double)(dy * dy + dx * dx + dz * dz));
		in.x = 0.0f;
		in.y = 0.0f;
		in.z = d2 * g_ATPCConvDist / d;
		const vec3_t *r = Vec3_Rotate(&out, &in, Entity_GetAng(v, &rot));
		origin.x = avg.x - r->x;
		origin.y = avg.y - r->y;
		origin.z = avg.z - r->z;
	} else {
		origin = hit;
	}
	for (int i = 0; i < 4; i++) {
		if (cTargetLock_HasSlot(wm, g_ATPCIDWeap, i)) {
			cVehicleEffects_Shoot(Vehicle_Effects(v), g_ATPCIDWeap, i);
		}
	}
	dx = origin.x - hit.x;
	dz = origin.z - hit.z;
	dy = origin.y - hit.y;
	mid.x = dx * 0.5f + origin.x;
	mid.y = dy * 0.5f + origin.y;
	mid.z = dz * 0.5f + origin.z;
	float len = (float)__builtin_sqrt((double)(dy * dy + dx * dx + dz * dz));
	if (FX_IsVisible(mid.x, mid.y, mid.z, len + 1000.0f, 250000.0f, NULL, 0)) {
		if (g_ATPCVictim[0]) {
			/* the hit moves to the side of the target the shooter sees */
			cEntity *w = World_FindEntity(g_ATPCVictim);
			CHECK(w);
			CHECK(Entity_IsType(w, TYEN_WITH_LIFE));
			float rad = Entity_Radius(w);
			const vec3_t *pc = WEnt_Vtbl(v)->GetCenter(v, &c);
			float cy = pc->y - hit.y;
			float cx = pc->x - hit.x;
			float cz = pc->z - hit.z;
			float l = (float)__builtin_sqrt((double)(cy * cy + cx * cx + cz * cz));
			if (l != 0.0f) {
				float inv = 1.0f / l;
				dir.x = cx * inv;
				dir.y = cy * inv;
				dir.z = cz * inv;
			} else {
				dir.x = cx;
				dir.y = cy;
				dir.z = cz;
			}
			float k = (float)(Weap_Rand01() * 0.40000003576278687 + 0.20000000298023224);
			hit.x = dir.x * rad * k + hit.x;
			hit.y = dir.y * rad * k + hit.y;
			hit.z = dir.z * rad * k + hit.z;
		}
		cDebrisSys *sys = cWorld_GetDebrisSys(g_pWorld, g_ATPCShootFile);
		cDebris *debris = cDebrisSys_NewElement(sys);
		debris->speed = g_ATPCTrailTime * 2.0f;
		cDebrisSys_Drop(sys, debris);
		cObjElementFXATPC *fx = Debris_Element(debris);
		if (fx) {
			if (!fx->on) {
				fx->on = 1;
			}
			cObjElementFXATPC_Shoot(fx, v, g_ATPCVictim, hit, origin);
		}
	}
	cFollowCamEntity *cam = Entity_FollowCam(v);
	if (cam) {
		int n = 0;
		for (int i = 0; i < 4; i++) {
			if (cTargetLock_HasSlot(wm, g_ATPCIDWeap, i)) {
				n++;
			}
		}
		float k = (float)n * 0.25f;
		cam->quakeTime = k * 0.2f + 0.25f;
		cam->quakeFactor = (float)((double)(k * 1.5f + 1.3f) * 3.141592653589793) / 180.0f;
	}
	cEntity *w = g_ATPCVictim[0] ? World_FindEntity(g_ATPCVictim) : NULL;
	if (w) {
		cSndItem *it = PtrArray_Get((byte *)w + 0x3a8, 6);
		if (it) {
			it->volume = it->vols[0];
			it->pitch = it->pitches[0];
			cSndItem_Play(it, v);
		}
	} else {
		ATPC_PlaySound(g_ATPCHitSound, &hit, g_ATPCHitSoundAttIni, g_ATPCHitSoundAttEnd, g_ATPCHitSoundVol);
	}

	/* the player near the line of the shot hears the capsule and shakes */
	cWorld *world = g_pWorld;
	if (world->owners[0]) {
		CHECK(Entity_IsType(world->owners[0], TYEN_WITH_LIFE));
	}
	cEntity *u = world->owners[0];
	if (!u || u == v || !Entity_IsType(u, TYEN_WITH_LIFE) || !Entity_FollowCam(u)) {
		return;
	}
	float dist = Dist_PointSegment(&origin, &hit, u->vftable->GetPos(u, &c), &near);
	int up = cWeaponManager_GetUpgrade(wm, g_ATPCIDWeap);
	float radius = g_ATPCCapsuleSoundRadius;
	if (up == 10) {
		radius = radius * 1.5f;
	}
	if (!(dist > 0.0f) || !(radius > dist)) {
		return;
	}
	float f = 1.0f - dist / radius;
	float t = (3.0f - (f + f)) * (f * f);
	cam = Entity_FollowCam(u);
	if (cam) {
		cam->quakeFactor = t * 0.0104719754f;
		cam->quakeTime = t * 0.04f;
	}
	if (up == 10) {
		ATPC_PlaySound(g_ATPCCapsuleExtendedSound, &near, g_ATPCCapsuleExtendedSoundAttIni,
			g_ATPCCapsuleExtendedSoundAttEnd, g_ATPCCapsuleExtendedSoundVol * t);
	} else {
		ATPC_PlaySound(g_ATPCCapsuleSound, &near, g_ATPCCapsuleSoundAttIni, g_ATPCCapsuleSoundAttEnd,
			g_ATPCCapsuleSoundVol * t);
	}
}

/* "!ATPCFX" (vehicle, player, item): the HUD item turns and grows while the ATPC recharges */
void ATPC_FX(cAbstractFile *params) {
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
	float stamp = Vehicle_WeapMgr(v)->fireStamp;
	float delay;
	if (cWeaponManager_GetUpgrade(Vehicle_WeapMgr(v), g_ATPCIDWeap) == 9) {
		delay = g_ATPCUpgradeDelay;
	} else {
		delay = g_ATPCDelay;
	}
	if (-0.25f > stamp) {
		float t = (stamp + 0.25f) / (0.25f - delay);
		Cannon_Color(item) = Weap_CannonColor();
		float size = 30.0f - t * 10.0f;
		Cannon_SizeX(item) = size;
		Cannon_SizeY(item) = size;
		Cannon_Angle(item) = t * ATPC_PI;
		return;
	}
	if (0.0f > stamp) {
		float s = stamp * -4.0f;
		int blue = (int)g_CannonScorerColorBlue - (int)(s * -255.0f);
		float size = s * 10.0f + 20.0f;
		uint32_t c = (uint32_t)(-0x41 - (int)(s * -64.0f));
		if (blue > 0xff) {
			blue = 0xff;
		}
		c = ((c << 8 | g_CannonScorerColorRed) << 8 | g_CannonScorerColorGreen) << 8 | (uint32_t)blue;
		Cannon_Color(item) = c;
		Cannon_SizeX(item) = size;
		Cannon_SizeY(item) = size;
	} else {
		Cannon_Color(item) = Weap_CannonColor();
		Cannon_SizeX(item) = 20.0f;
		Cannon_SizeY(item) = 20.0f;
	}
	Cannon_Angle(item) = 0.0f;
}

/* "!ATPCFire" (vehicle): the ammo of the shot, the aim and the line of the hit */
void ATPC_Fire(cAbstractFile *params) {
	char name[128];
	float ang[3];
	vec3_t hit, a, b, c, t, from;
	cEntity *target;
	float dist;

	cAbstractFile_ReadString(params, name, 0x80);
	memcpy(g_ATPCShooter, name, strlen(name) + 1);
	cEntity *v = Weap_GetVehicle(name);
	ATPC_SetRecharge(v);
	float conv = 1.0f;
	Controls_RumbleA(Vehicle_Controls(v), 1.0f, 0.2f);
	Controls_RumbleB(Vehicle_Controls(v), 1.0f, 0.5f);
	if (g_pApp->flags & APPF_CLIENT) {
		return;
	}
	cWeaponManager *wm = Vehicle_WeapMgr(v);
	g_ATPCNumSlots = 0.0f;
	for (int i = 0; i < 4; i++) {
		if (cTargetLock_HasSlot(wm, g_ATPCIDWeap, i)) {
			g_ATPCNumSlots = g_ATPCNumSlots + 1.0f;
		}
	}
	float rest = Weap_X87Sub(cTargetLock_GetAmmo(wm, g_ATPCIDAmmo), g_ATPCAmmoCost * g_ATPCNumSlots);
	if (0.0f > rest) {
		g_ATPCNumSlots = Weap_X87Div(cTargetLock_GetAmmo(wm, g_ATPCIDAmmo), g_ATPCAmmoCost);
		rest = 0.0f;
	}
	cWeaponManager_SetAmmo(wm, g_ATPCIDAmmo, rest);

	ang[0] = Veh(v)->autoAim.ax;
	ang[1] = Veh(v)->autoAim.ay;
	ang[2] = 0.0f;
	target = NULL;
	dist = g_ATPCRange;
	v->vftable->GetTargetAngle(v, ang, g_ATPCRange, &target);
	v->vftable->AimWeapon(v, ang[0], ang[1], &dist, &hit, &target, 8);
	if (!(g_ATPCConvDist > dist)) {
		conv = g_ATPCConvDist / dist;
	}
	if (target) {
		hit = *target->vftable->GetPos(target, &t);
		if (target && Entity_IsType(target, TYEN_VEHICLE)) {
			cEntity *tg = target;
			cVehicleAttack_ShotAt(Vehicle_Attack(v), tg->name, 1);
			const vec3_t *pa = v->vftable->GetPos(v, &a);
			const vec3_t *pb = tg->vftable->GetPos(tg, &b);
			float dx = (pb->x - pa->x) * conv;
			float dy = (pb->y - pa->y) * conv;
			float dz = (pb->z - pa->z) * conv;
			const vec3_t *pc = v->vftable->GetPos(v, &c);
			from.x = dx + pc->x;
			from.y = pc->y + dy;
			from.z = pc->z + dz;
			cWorld_LineCall(g_pWorld, 8, &from, tg->vftable->GetPos(tg, &t), g_ATPCExplosionRadius,
				"!ATPCXplosionCall");
			goto upgrade;
		}
	}
	cVehicleAttack_ShotAtPoint(Vehicle_Attack(v), &hit, 1);
	{
		const vec3_t *pa = v->vftable->GetPos(v, &t);
		float dx = (hit.x - pa->x) * conv;
		float dy = (hit.y - pa->y) * conv;
		float dz = (hit.z - pa->z) * conv;
		const vec3_t *pc = v->vftable->GetPos(v, &c);
		from.x = pc->x + dx;
		from.y = pc->y + dy;
		from.z = pc->z + dz;
		cWorld_LineCall(g_pWorld, 8, &from, &hit, g_ATPCExplosionRadius, "!ATPCXplosionCall");
	}
upgrade:;
	/* the upgrades 9 and 10 use up their own ammo, at 0 the script restores the weapon */
	int up = cWeaponManager_GetUpgrade(wm, g_ATPCIDWeap);
	if ((uint32_t)(up - 9) > 1) {
		return;
	}
	float u = Weap_X87Sub(cWeaponManager_GetUAmmo(wm, g_ATPCIDWeap), g_ATPCNumSlots * 10.0f);
	if (0.0f >= u) {
		cWeaponManager_SetUpgrade(wm, g_ATPCIDWeap, 0);
		cPy_Function_AddParamI(g_RestoreUpgradeFunc, up);
		cPy_Function_Execute(g_RestoreUpgradeFunc, 0);
		u = 0.0f;
	}
	cWeaponManager_SetUAmmo(wm, g_ATPCIDWeap, u);
}

/* "!ATPCOnEnd" (vehicle) */
void ATPC_OnEnd(cAbstractFile *params) {
	char name[128];

	cAbstractFile_ReadString(params, name, 0x80);
	cEntity *v = Weap_GetVehicle(name);
	Vehicle_AimMode(v) = 0;
}

/* "!ATPCOnStart" (vehicle): the aim help of the ATPC, wider for a pad with VehicleCenterPadLock */
void ATPC_OnStart(cAbstractFile *params) {
	char name[128];

	cAbstractFile_ReadString(params, name, 0x80);
	cEntity *v = Weap_GetVehicle(name);
	Vehicle_AimMode(v) = 4;
	cAutoAim_SetRange(Vehicle_AutoAim(v), g_ATPCRange);
	float ang;
	if (g_VehicleCenterPadLock != 0.0f && cVehicleEntity_GetOwnerSlot(v) != -1) {
		ang = 0.34906584f;
	} else {
		ang = 0.31415927f;
	}
	cAutoAim_SetAngle(Vehicle_AutoAim(v), ang);
}

/* "!ATPCXplosionCall" (entity, center, distance): the damage and push of the shot of g_ATPCShooter */
void ATPC_XplosionCall(cAbstractFile *params) {
	vec3_t center, p, r, n;
	float dist;

	cAbstractFile_ReadString(params, g_ATPCVictim, 0x80);
	params->vftable->Read(params, &center.x, 4);
	params->vftable->Read(params, &center.y, 4);
	params->vftable->Read(params, &center.z, 4);
	params->vftable->Read(params, &dist, 4);
	cEntity *victim = World_FindEntity(g_ATPCVictim);
	cEntity *shooter = World_FindEntity(g_ATPCShooter);
	if (!victim || !Entity_IsType(victim, TYEN_WITH_LIFE) || (g_pApp->flags & APPF_CLIENT)) {
		return;
	}
	int oldLife = Entity_LifeI(victim);
	int dmg = 0;
	byte ext = shooter && cWeaponManager_GetUpgrade(Vehicle_WeapMgr(shooter), g_ATPCIDWeap) == 10;
	const vec3_t *pp = victim->vftable->GetPos(victim, &p);
	float inv = 1.0f / dist;
	float dx = (pp->x - center.x) * inv;
	float dy = (pp->y - center.y) * inv;
	float dz = (pp->z - center.z) * inv;
	if (1.0f > dist) {
		/* too near: a random direction */
		double r1 = Weap_Rand01();
		double r2 = Weap_Rand01();
		double r3 = Weap_Rand01();
		p.x = (float)((r3 + r3) - 1.0);
		p.y = (float)((r2 + r2) - 1.0);
		p.z = (float)((r1 + r1) - 1.0);
		const vec3_t *d = Vec3_NormalizeTo(&r, &p);
		dx = d->x;
		dy = d->y;
		dz = d->z;
		dist = 1.0f;
	}
	dist = dist - Entity_Radius(victim);
	if (dist > g_ATPCExplosionRadius) {
		return;
	}
	if (1.0f > dist) {
		if (!ext) {
			dmg = (int)((float)g_ATPCMainDamage * g_ATPCNumSlots);
		}
		dist = 0.0f;
	}
	float f = (g_ATPCExplosionRadius - dist) / g_ATPCExplosionRadius;
	if (ext) {
		f = f * ((float)(g_ATPCMainDamage / g_ATPCExtraDamage) + 1.0f);
	}
	dmg += (int)((float)g_ATPCExtraDamage * f * g_ATPCNumSlots);
	if (shooter && shooter->vftable->GetAI(shooter) && World_IsEasySingle()) {
		dmg = dmg / 2;
		if (dmg <= 0) {
			dmg = 1;
		}
	}
	if (!Entity_God(victim)) {
		Entity_LifeI(victim) -= dmg;
	}
	if (oldLife > Entity_LifeI(victim)) {
		cVehicleAttack_HitBy(Vehicle_Attack(victim), g_ATPCShooter);
	}
	if (!Entity_IsType(victim, TYEN_WALKCHAR)) {
		float k = g_ATPCNumSlots * 1000.0f * g_ATPCHitForce / Entity_Mass(victim);
		dx = dx * f * k;
		dy = dy * f * k;
		dz = dz * f * k;
		const vec3_t *vel = WEnt_Vtbl(victim)->GetVel(victim, &r);
		n.x = dx + vel->x;
		n.y = vel->y + dy;
		n.z = vel->z + dz;
		Entity_SetVel(victim, &n);
	}
	if (Entity_IsType(victim, TYEN_VEHICLE) && Vehicle_NetCtrl(victim)) {
		Vehicle_NetCtrl(victim)->hit = 2;
	}
}

/* "!CloudFire" (vehicle): the script makes the debris cloud with the ammo of the vehicle */
void Cloud_Fire(cAbstractFile *params) {
	char name[128], cloudName[128];
	vec3_t pos, ang;

	cAbstractFile_ReadString(params, name, 0x80);
	cEntity *v = World_FindEntity(name);
	if (!v || !Entity_IsType(v, TYEN_VEHICLE)) {
		return;
	}
	float fammo = cTargetLock_GetAmmo(Vehicle_WeapMgr(v), g_CloudIDAmmo);
	int ammo = (int)fammo;
	if (Vehicle_CloudTime(v) > 0.0f || ammo < 1) {
		return;
	}
	Vehicle_WeapMgr(v)->cmStamp = g_CloudRechargeTime;
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
	Net_GetObjName(cloudName);
	v->vftable->GetPos(v, &pos);
	Entity_GetAng(v, &ang);
	cPy_Function_AddParamS(g_CreateCloudFunc, cloudName);
	cPy_Function_AddParamF(g_CreateCloudFunc, pos.x);
	cPy_Function_AddParamF(g_CreateCloudFunc, pos.y);
	cPy_Function_AddParamF(g_CreateCloudFunc, pos.z);
	cPy_Function_AddParamF(g_CreateCloudFunc, ang.x);
	cPy_Function_AddParamF(g_CreateCloudFunc, ang.y);
	cPy_Function_AddParamF(g_CreateCloudFunc, ang.z);
	cPy_Function_Execute(g_CreateCloudFunc, 0);
	cEntity *cloud = World_FindEntity(cloudName);
	if (!cloud) {
		return;
	}
	cMissileEntity_SetSpeed((cMissileEntity *)cloud, 0.0f);
	cMissileEntity_SetTurnSpeed((cMissileEntity *)cloud, 0.0f);
	Controls_RumbleA(Vehicle_Controls(v), 0.4f, 0.2f);
	cMissileEntity_SetCount((cMissileEntity *)cloud, g_CloudAmmoNeed <= ammo ? g_CloudAmmoNeed : ammo);
	cPy_Function_AddParamS(g_SetUpCloudFunc, cloud->name);
	cPy_Function_Execute(g_SetUpCloudFunc, 0);
	cMissileEntity_SetLifeTime((cMissileEntity *)cloud, g_CloudLifeTime - 0.2f);
	cPy_Function_SetFunc(Missile_OnHit(cloud), "!CloudOnHit");
	cWeaponManager_SetAmmo(Vehicle_WeapMgr(v), g_CloudIDAmmo, (float)(ammo - Missile_SwarmCount(cloud)));
}

/* "!CloudOnHit" (cloud, ?, entity, strong): a vehicle in the cloud for 100 or 5 seconds, a missile slowed */
void Cloud_OnHit(cAbstractFile *params) {
	char cloud[128], other[128], name[128];
	int strong = 0;

	cAbstractFile_ReadString(params, cloud, 0x80);
	cAbstractFile_ReadString(params, other, 0x80);
	cAbstractFile_ReadString(params, name, 0x80);
	params->vftable->Read(params, &strong, 4);
	cEntity *e = World_FindEntity(name);
	if (!e) {
		return;
	}
	if (Entity_IsType(e, TYEN_VEHICLE)) {
		cVehicleEntity_SetCloud(e, strong ? 100.0f : 5.0f, 1);
		return;
	}
	if (Entity_IsType(e, "Missile") && strong) {
		cMissileEntity_SetType((cMissileEntity *)e, 3);
		cMissileEntity_SetSwarmSpeed((cMissileEntity *)e, Missile_SwarmSpeed(e) / 5.0f);
	}
}
