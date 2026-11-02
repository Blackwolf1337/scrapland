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
#include "scrap/staticinit.h"

#include <string.h>

#define g_ATPCDelay (GVar_ATPCDelay.value.f)
#define g_ATPCUpgradeDelay (GVar_ATPCUpgradeDelay.value.f)
#define g_ATPCAmmoCost (GVar_ATPCAmmoCost.value.f)
#define g_ATPCExtraDamage (GVar_ATPCExtraDamage.value.i)
#define g_ATPCExplosionRadius (GVar_ATPCExplosionRadius.value.f)
#define g_ATPCMainDamage (GVar_ATPCMainDamage.value.i)
#define g_ATPCHitForce (GVar_ATPCHitForce.value.f)
#define g_ATPCConvDist (GVar_ATPCConvDist.value.f)
#define g_ATPCCapsuleSoundAttEnd (GVar_ATPCCapsuleSoundAttEnd.value.f)
#define g_ATPCCapsuleSoundAttIni (GVar_ATPCCapsuleSoundAttIni.value.f)
#define g_ATPCCapsuleSound (GVar_ATPCCapsuleSound.value.s)
#define g_ATPCCapsuleSoundRadius (GVar_ATPCCapsuleSoundRadius.value.f)
#define g_ATPCCapsuleExtendedSound (GVar_ATPCCapsuleExtendedSound.value.s)
#define g_ATPCHitSound (GVar_ATPCHitSound.value.s)
#define g_ATPCHitSoundAttEnd (GVar_ATPCHitSoundAttEnd.value.f)
#define g_ATPCHitSoundAttIni (GVar_ATPCHitSoundAttIni.value.f)
#define g_ATPCHitSoundVol (GVar_ATPCHitSoundVol.value.f)
#define g_ATPCCapsuleSoundVol (GVar_ATPCCapsuleSoundVol.value.f)
#define g_ATPCShootFile (GVar_ATPCShootFile.value.s)
#define g_ATPCCapsuleExtendedSoundAttIni (GVar_ATPCCapsuleExtendedSoundAttIni.value.f)
#define g_ATPCCapsuleExtendedSoundAttEnd (GVar_ATPCCapsuleExtendedSoundAttEnd.value.f)
#define g_ATPCCapsuleExtendedSoundVol (GVar_ATPCCapsuleExtendedSoundVol.value.f)
#define g_CreateCloudFunc (((cPy_Function *)&GVar_CreateCloudFunc.func))
#define g_CloudLifeTime (GVar_CloudLifeTime.value.f)
#define g_CloudAmmoNeed (GVar_CloudAmmoNeed.value.i)
#define g_CloudIDAmmo (GVar_CloudIDAmmo.value.i)
#define g_CloudRechargeTime (GVar_CloudRechargeTime.value.f)
#define g_SetUpCloudFunc (((cPy_Function *)&GVar_SetUpCloudFunc.func))
static cGVarFunction GVar_SetUpCloudFunc;
static cGVarFunction GVar_CreateCloudFunc;
static cGVar GVar_CloudRechargeTime;
static cGVar GVar_CloudLifeTime;
static cGVar GVar_CloudIDAmmo;
static cGVar GVar_CloudAmmoNeed;
static cGVar GVar_ATPCUpgradeDelay;
static cGVar GVar_ATPCShootFile;
static cGVar GVar_ATPCMainDamage;
static cGVar GVar_ATPCHitSoundVol;
static cGVar GVar_ATPCHitSoundAttIni;
static cGVar GVar_ATPCHitSoundAttEnd;
static cGVar GVar_ATPCHitSound;
static cGVar GVar_ATPCHitForce;
static cGVar GVar_ATPCExtraDamage;
static cGVar GVar_ATPCExplosionRadius;
static cGVar GVar_ATPCDelay;
static cGVar GVar_ATPCConvDist;
static cGVar GVar_ATPCCapsuleSoundVol;
static cGVar GVar_ATPCCapsuleSoundRadius;
static cGVar GVar_ATPCCapsuleSoundAttIni;
static cGVar GVar_ATPCCapsuleSoundAttEnd;
static cGVar GVar_ATPCCapsuleSound;
static cGVar GVar_ATPCCapsuleExtendedSoundVol;
static cGVar GVar_ATPCCapsuleExtendedSoundAttIni;
static cGVar GVar_ATPCCapsuleExtendedSoundAttEnd;
static cGVar GVar_ATPCCapsuleExtendedSound;
static cGVar GVar_ATPCAmmoCost;

#define ATPC_PI 3.14159274f

/* the recharge of the next shot (the upgrade 9 is faster) and the start sound of the upgrade */
static void ATPC_SetRecharge(cEntity *v) {
	int up = cWeaponManager_GetUpgrade(&Veh(v)->weapons, g_ATPCIDWeap);
	float ammo = cTargetLock_GetAmmo(&Veh(v)->weapons, g_ATPCIDAmmo);
	int snd;
	if (ammo > 0.0f) {
		if (up == 9) {
			Veh(v)->weapons.fireStamp = -g_ATPCUpgradeDelay;
			Vehicle_StartSnd(v, 0x11);
			return;
		}
		Veh(v)->weapons.fireStamp = -g_ATPCDelay;
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

/* Weaps/ATPC.cpp: the shot of the ATPC (a charge per mounted slot), its HUD item and the debris cloud */
static char g_ATPCShooter[128];
static char g_ATPCVictim[128];

/* "!ATPCCreate" (vehicle, hit point, target): the shot effect from the mounted slots (beyond the convergence distance
 * from their middle) to the hit, the hit sound and the capsule sound and shake for the player near the shot */
static void ATPC_Create(cAbstractFile *params) {
	vec3_t hit, a, p, avg, in, rot, out, origin, mid, c, dir, near;
	matrix_t mat;

	cAbstractFile_ReadString(params, g_ATPCShooter, 0x80);
	params->vftable->Read(params, &hit.x, 4);
	params->vftable->Read(params, &hit.y, 4);
	params->vftable->Read(params, &hit.z, 4);
	cAbstractFile_ReadString(params, g_ATPCVictim, 0x80);
	cEntity *v = Weap_GetVehicle(g_ATPCShooter);
	cWeaponManager *wm = &Veh(v)->weapons;
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
			const matrix_t *m = CGameModel_NodeMatrix(Obj_Vtbl(v)->GetModel(v), &mat, node, 0, NULL, 0);
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
		const vec3_t *r = Vec3_Rotate(&out, &in, Obj_Vtbl(v)->GetAng(v, &rot));
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
			float rad = WLife(w)->prim.radius;
			const vec3_t *pc = Obj_Vtbl(v)->GetViewPos(v, &c);
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
		debris->life = g_ATPCTrailTime * 2.0f;
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
		cSndItem *it = PtrArray_Get(&WLife(w)->hitSounds, 6);
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
static void ATPC_FX(cAbstractFile *params) {
	char name[128], itemName[128];
	int player;

	cAbstractFile_ReadString(params, name, 0x80);
	params->vftable->Read(params, &player, 4);
	cAbstractFile_ReadString(params, itemName, 0x80);
	cEntity *v = Weap_GetVehicle(name);
	cCannonScorer *item = (cCannonScorer *)cScorer_FindItem(Scorer_Get(player), itemName);
	if (!item) {
		return;
	}
	float stamp = Veh(v)->weapons.fireStamp;
	float delay;
	if (cWeaponManager_GetUpgrade(&Veh(v)->weapons, g_ATPCIDWeap) == 9) {
		delay = g_ATPCUpgradeDelay;
	} else {
		delay = g_ATPCDelay;
	}
	if (-0.25f > stamp) {
		float t = (stamp + 0.25f) / (0.25f - delay);
		item->color = Weap_CannonColor();
		float size = 30.0f - t * 10.0f;
		item->size[0] = size;
		item->size[1] = size;
		item->angle = t * ATPC_PI;
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
		item->color = c;
		item->size[0] = size;
		item->size[1] = size;
	} else {
		item->color = Weap_CannonColor();
		item->size[0] = 20.0f;
		item->size[1] = 20.0f;
	}
	item->angle = 0.0f;
}

static float g_ATPCNumSlots;

/* "!ATPCFire" (vehicle): the ammo of the shot, the aim and the line of the hit */
static void ATPC_Fire(cAbstractFile *params) {
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
	Controls_RumbleA(&Veh(v)->controls, 1.0f, 0.2f);
	Controls_RumbleB(&Veh(v)->controls, 1.0f, 0.5f);
	if (g_pApp->flags & APPF_CLIENT) {
		return;
	}
	cWeaponManager *wm = &Veh(v)->weapons;
	g_ATPCNumSlots = 0.0f;
	for (int i = 0; i < 4; i++) {
		if (cTargetLock_HasSlot(wm, g_ATPCIDWeap, i)) {
			g_ATPCNumSlots = g_ATPCNumSlots + 1.0f;
		}
	}
	float rest = (float)((cTargetLock_GetAmmo(wm, g_ATPCIDAmmo)) - (g_ATPCAmmoCost * g_ATPCNumSlots));
	if (0.0f > rest) {
		g_ATPCNumSlots = (float)((cTargetLock_GetAmmo(wm, g_ATPCIDAmmo)) / g_ATPCAmmoCost);
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
			cVehicleAttack_ShotAt(&WLife(v)->attack, tg->name, 1);
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
	cVehicleAttack_ShotAtPoint(&WLife(v)->attack, &hit, 1);
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
	float u = (float)((cWeaponManager_GetUAmmo(wm, g_ATPCIDWeap)) - (g_ATPCNumSlots * 10.0f));
	if (0.0f >= u) {
		cWeaponManager_SetUpgrade(wm, g_ATPCIDWeap, 0);
		cPy_Function_AddParamI(g_RestoreUpgradeFunc, up);
		cPy_Function_Execute(g_RestoreUpgradeFunc, 0);
		u = 0.0f;
	}
	cWeaponManager_SetUAmmo(wm, g_ATPCIDWeap, u);
}

/* "!ATPCOnEnd" (vehicle) */
static void ATPC_OnEnd(cAbstractFile *params) {
	char name[128];

	cAbstractFile_ReadString(params, name, 0x80);
	cEntity *v = Weap_GetVehicle(name);
	Veh(v)->autoAim.status = 0;
}

/* "!ATPCOnStart" (vehicle): the aim help of the ATPC, wider for a pad with VehicleCenterPadLock */
static void ATPC_OnStart(cAbstractFile *params) {
	char name[128];

	cAbstractFile_ReadString(params, name, 0x80);
	cEntity *v = Weap_GetVehicle(name);
	Veh(v)->autoAim.status = 4;
	cAutoAim_SetRange(&Veh(v)->autoAim, g_ATPCRange);
	float ang;
	if (g_VehicleCenterPadLock != 0.0f && cVehicleEntity_GetOwnerSlot(v) != -1) {
		ang = 0.34906584f;
	} else {
		ang = 0.31415927f;
	}
	cAutoAim_SetAngle(&Veh(v)->autoAim, ang);
}

/* "!ATPCXplosionCall" (entity, center, distance): the damage and push of the shot of g_ATPCShooter */
static void ATPC_XplosionCall(cAbstractFile *params) {
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
	int oldLife = WLife(victim)->life;
	int dmg = 0;
	byte ext = shooter && cWeaponManager_GetUpgrade(&Veh(shooter)->weapons, g_ATPCIDWeap) == 10;
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
	dist = dist - WLife(victim)->prim.radius;
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
	if (!WLife(victim)->god) {
		WLife(victim)->life -= dmg;
	}
	if (oldLife > WLife(victim)->life) {
		cVehicleAttack_HitBy(&WLife(victim)->attack, g_ATPCShooter);
	}
	if (!Entity_IsType(victim, TYEN_WALKCHAR)) {
		float k = g_ATPCNumSlots * 1000.0f * g_ATPCHitForce / WLife(victim)->mass;
		dx = dx * f * k;
		dy = dy * f * k;
		dz = dz * f * k;
		const vec3_t *vel = Obj_Vtbl(victim)->GetVel(victim, &r);
		n.x = dx + vel->x;
		n.y = vel->y + dy;
		n.z = vel->z + dz;
		Obj_Vtbl(victim)->SetVel(victim, &n);
	}
	if (Entity_IsType(victim, TYEN_VEHICLE) && Veh(victim)->netCtrl) {
		Veh(victim)->netCtrl->hit = 2;
	}
}

/* "!CloudFire" (vehicle): the script makes the debris cloud with the ammo of the vehicle */
static void Cloud_Fire(cAbstractFile *params) {
	char name[128], cloudName[128];
	vec3_t pos, ang;

	cAbstractFile_ReadString(params, name, 0x80);
	cEntity *v = World_FindEntity(name);
	if (!v || !Entity_IsType(v, TYEN_VEHICLE)) {
		return;
	}
	float fammo = cTargetLock_GetAmmo(&Veh(v)->weapons, g_CloudIDAmmo);
	int ammo = (int)fammo;
	if (Veh(v)->cloud > 0.0f || ammo < 1) {
		return;
	}
	Veh(v)->weapons.cmStamp = g_CloudRechargeTime;
	cSndItem *it = PtrArray_Get(&Veh(v)->wl.obj.snd, 0x13);
	if (it) {
		cSndItem *src = PtrArray_Get(&Veh(v)->wl.obj.snd, 9);
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
	Obj_Vtbl(v)->GetAng(v, &ang);
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
	Controls_RumbleA(&Veh(v)->controls, 0.4f, 0.2f);
	cMissileEntity_SetCount((cMissileEntity *)cloud, g_CloudAmmoNeed <= ammo ? g_CloudAmmoNeed : ammo);
	cPy_Function_AddParamS(g_SetUpCloudFunc, cloud->name);
	cPy_Function_Execute(g_SetUpCloudFunc, 0);
	cMissileEntity_SetLifeTime((cMissileEntity *)cloud, g_CloudLifeTime - 0.2f);
	cPy_Function_SetFunc(&Missile(cloud)->onHit, "!CloudOnHit");
	cWeaponManager_SetAmmo(&Veh(v)->weapons, g_CloudIDAmmo, (float)(ammo - Missile(cloud)->swarmCount));
}

/* "!CloudOnHit" (cloud, ?, entity, strong): a vehicle in the cloud for 100 or 5 seconds, a missile slowed */
static void Cloud_OnHit(cAbstractFile *params) {
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
		cMissileEntity_SetSwarmSpeed((cMissileEntity *)e, Missile(e)->swarmSpeed / 5.0f);
	}
}

cGVarFunction GVar_RestoreUpgradeFunc;
cGVar GVar_ATPCIDWeap;
static cGVar GVar_ATPCCapsuleSoundAttEnd;
static cGVar GVar_ATPCCapsuleSoundAttIni;
static cGVar GVar_ATPCUpgradeDelay;
static cGVar GVar_ATPCCapsuleSound;
static cGVar GVar_ATPCCapsuleSoundRadius;
static cGVar GVar_ATPCAmmoCost;
static cGVar GVar_ATPCExtraDamage;
static cGVar GVar_ATPCExplosionRadius;
static cGVar GVar_ATPCCapsuleExtendedSound;
static cGVar GVar_ATPCHitSound;
static cGVar GVar_ATPCMainDamage;
static cGVar GVar_ATPCHitSoundAttEnd;
static cGVar GVar_ATPCHitSoundAttIni;
static cGVar GVar_ATPCHitSoundVol;
static cGVar GVar_ATPCDelay;
static cGVar GVar_ATPCHitForce;
static cGVar GVar_ATPCCapsuleSoundVol;
cGVar GVar_ATPCRange;
static cGVar GVar_ATPCConvDist;
static cGVar GVar_ATPCShootFile;
static cGVar GVar_ATPCCapsuleExtendedSoundAttIni;
static cGVar GVar_ATPCCapsuleExtendedSoundAttEnd;
static cGVar GVar_ATPCCapsuleExtendedSoundVol;
cGVar GVar_ATPCIDAmmo;
static cGVarFunction GVar_CreateCloudFunc;
static cGVar GVar_CloudLifeTime;
static cGVar GVar_CloudAmmoNeed;
static cGVar GVar_CloudIDAmmo;
static cGVar GVar_CloudRechargeTime;
static cGVarFunction GVar_SetUpCloudFunc;
static cPy_NativeFunc NativeFunc_ATPCOnEnd;
static cPy_NativeFunc NativeFunc_ATPCOnStart;
static cPy_NativeFunc NativeFunc_ATPCXplosionCall;
static cPy_NativeFunc NativeFunc_ATPCCreate;
static cPy_NativeFunc NativeFunc_ATPCFire;
static cPy_NativeFunc NativeFunc_ATPCFX;
static cPy_NativeFunc NativeFunc_CloudOnHit;
static cPy_NativeFunc NativeFunc_CloudFire;

/* the dynamic initializers of the globals here, run in the order of src/standalone/crtxc.inc */
FINI_CALL(Fini_GVar_RestoreUpgradeFunc, &GVar_RestoreUpgradeFunc.func, cPy_Function_dtor)
INIT_GVAR_FUNCTION(Init_GVar_RestoreUpgradeFunc, Fini_GVar_RestoreUpgradeFunc, &GVar_RestoreUpgradeFunc, "RestoreUpgradeFunc", 0, "Function to be called when an upgrade is restored", "Items.RestoreUpgrade")
INIT_GVAR_INT(Init_GVar_ATPCIDWeap, &GVar_ATPCIDWeap, "ATPCIDWeap", 0, "The default weapon idx for the ATPC", 4)
INIT_GVAR_INT(Init_GVar_ATPCIDAmmo, &GVar_ATPCIDAmmo, "ATPCIDAmmo", 0, "The default weapon idx for the ATPC", 0)
FINI_GVAR_STRING(Fini_GVar_ATPCShootFile, &GVar_ATPCShootFile)
INIT_GVAR_STRING(Init_GVar_ATPCShootFile, Fini_GVar_ATPCShootFile, &GVar_ATPCShootFile, "ATPCShootFile", 0, "default shoot model for the ATPC", "Models/Weapons/ATPC/Shoot.M3D")
INIT_GVAR_FLOAT(Init_GVar_ATPCHitForce, &GVar_ATPCHitForce, "ATPCHitForce", 0, "The force that have a single ATPC shoot", 10000.0f)
INIT_GVAR_FLOAT(Init_GVar_ATPCRange, &GVar_ATPCRange, "ATPCRange", 0, "The maximun range that have the ATPC", 100000.0f)
INIT_GVAR_FLOAT(Init_GVar_ATPCAmmoCost, &GVar_ATPCAmmoCost, "ATPCAmmoCost", 0, "Cost Per hit at ATPC", 10.0f)
INIT_GVAR_FLOAT(Init_GVar_ATPCDelay, &GVar_ATPCDelay, "ATPCDelay", 0, "Recharge time ATPC", 2.5f)
INIT_GVAR_FLOAT(Init_GVar_ATPCUpgradeDelay, &GVar_ATPCUpgradeDelay, "ATPCUpgradeDelay", 0, "Recharge time ATPC (with upgrade)", 1.75f)
INIT_GVAR_FLOAT(Init_GVar_ATPCConvDist, &GVar_ATPCConvDist, "ATPCConvDist", 0, "Convergence distance (and Start event)", 2000.5f)
INIT_GVAR_FLOAT(Init_GVar_ATPCExplosionRadius, &GVar_ATPCExplosionRadius, "ATPCExplosionRadius", 0, "Radius for the capsule hit for ATPC", 1000.0f)
INIT_GVAR_FLOAT(Init_GVar_ATPCCapsuleSoundRadius, &GVar_ATPCCapsuleSoundRadius, "ATPCCapsuleSoundRadius", 0, "Radius for the Sound Capsule for ATPC", 6000.0f)
INIT_GVAR_INT(Init_GVar_ATPCMainDamage, &GVar_ATPCMainDamage, "ATPCMainDamage", 0, "The hit damage for ATPC", 20)
INIT_GVAR_INT(Init_GVar_ATPCExtraDamage, &GVar_ATPCExtraDamage, "ATPCExtraDamage", 0, "The xplosion damage for ATPC", 10)
FINI_GVAR_STRING(Fini_GVar_ATPCHitSound, &GVar_ATPCHitSound)
INIT_GVAR_STRING(Init_GVar_ATPCHitSound, Fini_GVar_ATPCHitSound, &GVar_ATPCHitSound, "ATPCHitSound", 0, "First ATPC puchuing sound name", "ATPCImpact")
INIT_GVAR_FLOAT(Init_GVar_ATPCHitSoundVol, &GVar_ATPCHitSoundVol, "ATPCHitSoundVol", 0, "First ATPC puchuing sound volume", 1.0f)
INIT_GVAR_FLOAT(Init_GVar_ATPCHitSoundAttIni, &GVar_ATPCHitSoundAttIni, "ATPCHitSoundAttIni", 0, "First ATPC puchuing att ini", 0.0f)
INIT_GVAR_FLOAT(Init_GVar_ATPCHitSoundAttEnd, &GVar_ATPCHitSoundAttEnd, "ATPCHitSoundAttEnd", 0, "First ATPC puchuing att end", 0.0f)
FINI_GVAR_STRING(Fini_GVar_ATPCCapsuleSound, &GVar_ATPCCapsuleSound)
INIT_GVAR_STRING(Init_GVar_ATPCCapsuleSound, Fini_GVar_ATPCCapsuleSound, &GVar_ATPCCapsuleSound, "ATPCCapsuleSound", 0, "ATPC capsule sound name", "ATPCCapsule")
INIT_GVAR_FLOAT(Init_GVar_ATPCCapsuleSoundVol, &GVar_ATPCCapsuleSoundVol, "ATPCCapsuleSoundVol", 0, "ATPC capsule sound volume", 1.0f)
INIT_GVAR_FLOAT(Init_GVar_ATPCCapsuleSoundAttIni, &GVar_ATPCCapsuleSoundAttIni, "ATPCCapsuleSoundAttIni", 0, "ATPC capsule sound att ini", 0.0f)
INIT_GVAR_FLOAT(Init_GVar_ATPCCapsuleSoundAttEnd, &GVar_ATPCCapsuleSoundAttEnd, "ATPCCapsuleSoundAttEnd", 0, "ATPC capsule sound att end", 0.0f)
FINI_GVAR_STRING(Fini_GVar_ATPCCapsuleExtendedSound, &GVar_ATPCCapsuleExtendedSound)
INIT_GVAR_STRING(Init_GVar_ATPCCapsuleExtendedSound, Fini_GVar_ATPCCapsuleExtendedSound, &GVar_ATPCCapsuleExtendedSound, "ATPCCapsuleExtendedSound", 0, "ATPC capsule extended sound name", "ATPCCapsuleExtended")
INIT_GVAR_FLOAT(Init_GVar_ATPCCapsuleExtendedSoundVol, &GVar_ATPCCapsuleExtendedSoundVol, "ATPCCapsuleExtendedSoundVol", 0, "ATPC capsule extended sound volume", 1.0f)
INIT_GVAR_FLOAT(Init_GVar_ATPCCapsuleExtendedSoundAttIni, &GVar_ATPCCapsuleExtendedSoundAttIni, "ATPCCapsuleExtendedSoundAttIni", 0, "ATPC capsule Extended sound att ini", 0.0f)
INIT_GVAR_FLOAT(Init_GVar_ATPCCapsuleExtendedSoundAttEnd, &GVar_ATPCCapsuleExtendedSoundAttEnd, "ATPCCapsuleExtendedSoundAttEnd", 0, "ATPC capsule Extended sound att end", 0.0f)
FINI_CALL(Fini_GVar_CreateCloudFunc, &GVar_CreateCloudFunc.func, cPy_Function_dtor)
INIT_GVAR_FUNCTION(Init_GVar_CreateCloudFunc, Fini_GVar_CreateCloudFunc, &GVar_CreateCloudFunc, "CreateCloudFunc", 0, "Function to be called to create the cloud", "")
FINI_CALL(Fini_GVar_SetUpCloudFunc, &GVar_SetUpCloudFunc.func, cPy_Function_dtor)
INIT_GVAR_FUNCTION(Init_GVar_SetUpCloudFunc, Fini_GVar_SetUpCloudFunc, &GVar_SetUpCloudFunc, "SetUpCloudFunc", 0, "Function to be called to set up the cloud properties", "")
INIT_GVAR_INT(Init_GVar_CloudAmmoNeed, &GVar_CloudAmmoNeed, "CloudAmmoNeed", 0, "The default cost ammunition for the cloud", 20)
INIT_GVAR_INT(Init_GVar_CloudIDAmmo, &GVar_CloudIDAmmo, "CloudIDAmmo", 0, "Cloud Ammo ID", 0)
INIT_GVAR_FLOAT(Init_GVar_CloudRechargeTime, &GVar_CloudRechargeTime, "CloudRechargeTime", 0, "Cloud Recharge Time", 0.0f)
INIT_GVAR_FLOAT(Init_GVar_CloudLifeTime, &GVar_CloudLifeTime, "CloudLifeTime", 0, "Cloud Life Time", 0.0f)
INIT_NATIVE_FUNC(Init_NativeFunc_ATPCOnEnd, &NativeFunc_ATPCOnEnd, "ATPCOnEnd", ATPC_OnEnd)
INIT_NATIVE_FUNC(Init_NativeFunc_ATPCOnStart, &NativeFunc_ATPCOnStart, "ATPCOnStart", ATPC_OnStart)
INIT_NATIVE_FUNC(Init_NativeFunc_ATPCXplosionCall, &NativeFunc_ATPCXplosionCall, "ATPCXplosionCall", ATPC_XplosionCall)
INIT_NATIVE_FUNC(Init_NativeFunc_ATPCCreate, &NativeFunc_ATPCCreate, "ATPCCreate", ATPC_Create)
INIT_NATIVE_FUNC(Init_NativeFunc_ATPCFire, &NativeFunc_ATPCFire, "ATPCFire", ATPC_Fire)
INIT_NATIVE_FUNC(Init_NativeFunc_ATPCFX, &NativeFunc_ATPCFX, "ATPCFX", ATPC_FX)
INIT_NATIVE_FUNC(Init_NativeFunc_CloudOnHit, &NativeFunc_CloudOnHit, "CloudOnHit", Cloud_OnHit)
INIT_NATIVE_FUNC(Init_NativeFunc_CloudFire, &NativeFunc_CloudFire, "CloudFire", Cloud_Fire)
