/*
 * Entities/Vehicle/Weaps/Devastator.cpp of the game: the native functions of the Devastator (a spread shot that
 * explodes at the hit, "!DevastatorXplosionCall") and of the EMI, see include/scrap/weapons.h. Also the native
 * "!InfernoClientFire" that hands the name to Weaps/Inferno.cpp.
 */
#include "scrap/ai.h"
#include "scrap/fxmisc.h"
#include "scrap/mathlib.h"
#include "scrap/net.h"
#include "scrap/scorer.h"
#include "scrap/weapons.h"
#include "scrap/weaps2.h"

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

/* entity vtable slot 0x78: the angles of the object (the rotation of Vec3_Rotate) */
static inline vec3_t *Dev_GetRot(cEntity *e, vec3_t *out) {
	return ENT_VF(e, 0x78, vec3_t * (__thiscall *)(cEntity *, vec3_t *))(e, out);
}

/* the sizes and the life of the explosion particles: rand01 * a + b as a float */
static inline float Dev_Rand(double a, double b) {
	return (float)(Weap_Rand01() * a + b);
}

/* a random float between -r and r */
static inline float Dev_RandSym(float r) {
	float lo = -r;
	return (float)(Weap_Rand01() * (double)(r - lo) + (double)lo);
}

/* "!DevastatorCreate" (vehicle, hit point into g_DevCenter, entity hit, shot number): the shot of the next mounted
 * slot (a bullet for a player), then where the effects are visible the explosion particles on the floor at the hit,
 * the shot trail and the sparks, the damage of the sphere around the hit (not on a net client) and the boom */
void Devastator_Create(cAbstractFile *params) {
	vec3_t c, r, in, rot, a, b, hit, n, p, origin, cam, d, u, q, e, mid;
	matrix_t mat;
	CColisTri tri;
	int shot;

	cAbstractFile_ReadString(params, g_DevShooter, 0x80);
	params->vftable->Read(params, &g_DevCenter->x, 4);
	params->vftable->Read(params, &g_DevCenter->y, 4);
	params->vftable->Read(params, &g_DevCenter->z, 4);
	cAbstractFile_ReadString(params, g_DevVictim, 0x80);
	params->vftable->Read(params, &shot, 4);
	cEntity *v = Weap_GetVehicle(g_DevShooter);
	byte *shake = Entity_Shake(v);
	cWeaponManager *wm = Vehicle_WeapMgr(v);
	int num = cWeaponManager_NumSlots(wm, g_DevastatorIDWeap, 1);
	if (!num) {
		return;
	}
	float q0 = Weap_X87Div(cTargetLock_GetAmmo(wm, g_DevastatorIDAmmo), g_DevastatorAmmoCost);
	int k = (int)((float)shot + q0) % num;
	/* the k-th mounted slot fires (the original keeps stack garbage as the origin without one) */
	origin = (vec3_t){ 0.0f, 0.0f, 0.0f };
	for (int i = 0; i < 4; i++) {
		if (!cTargetLock_HasSlot(wm, g_DevastatorIDWeap, i)) {
			continue;
		}
		if (k) {
			k--;
			continue;
		}
		int node = wm->nodes[g_DevastatorIDAmmo][i];
		const matrix_t *m = CGameModel_NodoMatrix(WEnt_Vtbl(v)->GetObjData(v), &mat, node, 0, NULL, 0);
		p.x = m->m[12];
		p.y = m->m[13];
		p.z = m->m[14];
		if (shake) {
			cDebrisSys *sys = cWorld_GetDebrisSys(g_pWorld, g_DevastatorBulletFile);
			cDebris *bullet = cDebrisSys_NewElement(sys);
			bullet->drop[0] = p;
			bullet->drop[1] = *Dev_GetRot(v, &rot);
			float rz = Dev_Rand(14.0, -7.0);
			float ry = Dev_Rand(2000.0, 0.0);
			float rx = Dev_Rand(200.0, -100.0);
			const vec3_t *pc = WEnt_Vtbl(v)->GetCenter(v, &rot);
			float dx = (bullet->drop[0].x - pc->x) * 10.0f;
			float dy = (bullet->drop[0].y - pc->y) * 10.0f;
			float dz = (bullet->drop[0].z - pc->z) * 10.0f;
			const vec3_t *vel = WEnt_Vtbl(v)->GetVel(v, &c);
			bullet->drop[2].x = dx + vel->x * 0.5f + rx;
			bullet->drop[2].z = dz + vel->z * 0.5f + rz;
			bullet->drop[2].y = dy + vel->y * 0.5f + ry;
			float sz = Dev_Rand(14.0, -7.0);
			float sy = Dev_Rand(14.0, -7.0);
			bullet->drop[3].x = Dev_Rand(14.0, -7.0);
			bullet->drop[3].y = sy;
			bullet->drop[3].z = sz;
			bullet->speed = 0.4f;
			cDebrisSys_Drop(sys, bullet);
		}
		cVehicleEffects_Shoot(Vehicle_Effects(v), g_DevastatorIDWeap, i);
		origin = p;
		break;
	}

	vec3_t *C = g_DevCenter;
	float dy = origin.y - C->y;
	float dx = origin.x - C->x;
	float dz = origin.z - C->z;
	mid.y = dy * 0.5f + origin.y;
	mid.z = dz * 0.5f + origin.z;
	mid.x = dx * 0.5f + origin.x;
	float len = (float)__builtin_sqrt((double)(dy * dy + dx * dx + dz * dz));
	if (FX_IsVisible(mid.x, mid.y, mid.z, len + g_DevastatorExplosionRadius * 1.5f, g_FXDevastatorDistance, NULL,
		0)) {
		if (g_DevVictim[0]) {
			/* the hit moves to the side of the target the shooter sees */
			cEntity *w = World_FindEntity(g_DevVictim);
			CHECK(w);
			CHECK(Entity_IsType(w, TYEN_WITH_LIFE));
			float rad = Entity_Radius(w);
			const vec3_t *pc = WEnt_Vtbl(v)->GetCenter(v, &c);
			float cy = pc->y - C->y;
			float cx = pc->x - C->x;
			float cz = pc->z - C->z;
			float l = (float)__builtin_sqrt((double)(cy * cy + cx * cx + cz * cz));
			if (l != 0.0f) {
				float inv = 1.0f / l;
				d.x = inv * cx;
				d.y = inv * cy;
				d.z = inv * cz;
			} else {
				d.x = cx;
				d.y = cy;
				d.z = cz;
			}
			float f = Dev_Rand(0x1.333334p-2, 0x1.99999ap-3);
			C->x = C->x + f * (d.x * rad);
			C->z = C->z + f * (d.z * rad);
			C->y = C->y + f * (d.y * rad);
		}
		cWorld *world = g_pWorld;
		cam = world->camPos[world->curCam];
		float ey = cam.y - C->y;
		float ex = cam.x - C->x;
		float ez = cam.z - C->z;
		float camDist = (float)__builtin_sqrt((double)(ey * ey + ex * ex + ez * ez));
		if (g_SplitScreenGame) {
			camDist = g_FXDevastatorDistance * 0.5f;
		}

		/* the explosion on the floor */
		cDebrisSys *sys = cWorld_GetDebrisSys(world, g_DevastatorHitFile);
		cDebris *dhit = cDebrisSys_GetIdle(sys);
		cam = g_pWorld->camPos[g_pWorld->curCam];
		CGameModel_SetPos(Debris_Mod(dhit), &cam);
		dhit->speed = 1.2f;
		if (!cDebrisSys_IsIdle(sys, dhit)) {
			cDebrisSys_Drop(sys, dhit);
		}
		cObjElementFXSistPart *fx = Debris_Element(dhit);
		if (!fx) {
			goto sphere;
		}
		if (!fx->on) {
			fx->move = 0;
			fx->on = 1;
			CGameModel_SetSortZ(Debris_Mod(dhit), -0.03f, 0.0f);
		}
		CGameParticles *pt = &fx->particles;
		/* the floor normal at the hit, else the up of the shooter */
		in = (vec3_t){ 0.0f, 0.0f, 25.0f };
		const vec3_t *ro = Vec3_Rotate(&r, &in, Dev_GetRot(v, &rot));
		a.x = C->x + ro->x;
		a.z = ro->z + C->z;
		a.y = ro->y + C->y;
		in = (vec3_t){ 0.0f, 0.0f, -25.0f };
		ro = Vec3_Rotate(&r, &in, Dev_GetRot(v, &rot));
		b.z = ro->z + C->z;
		b.x = ro->x + C->x;
		b.y = ro->y + C->y;
		if (Engine_TestLine(&a, &b, &hit, &tri)) {
			n = tri.plane.n;
		} else {
			in = (vec3_t){ 0.0f, 0.0f, 1.0f };
			n = *Vec3_Rotate(&r, &in, Dev_GetRot(v, &rot));
		}
		int up = cWeaponManager_GetUpgrade(wm, g_DevastatorIDWeap);
		float t = 1.0f - camDist / g_FXDevastatorDistance;
		float scale = Float_MaxSS(1.0f, t * t * (camDist / 4000.0f));
		if (up == 7) {
			scale = scale * 1.5f;
		}
		if (g_DevVictim[0]) {
			/* larger and a shake when the player is hit */
			cEntity *w = World_FindEntity(g_DevVictim);
			if (w && w == cWorld_GetUsrEntity(g_pWorld, 0) && Entity_IsType(w, TYEN_WITH_LIFE) && Entity_Shake(w)) {
				byte *ws = Entity_Shake(w);
				scale = scale + scale;
				*(float *)(ws + 0x224) = 0x1.015bf8p-5f;
				*(float *)(ws + 0x220) = 0.2f;
			}
		}
		float R = g_FXDevastatorRadius;
		p.x = R * n.x * 0.5f * scale + C->x;
		p.y = R * n.y * 0.5f * scale + C->y;
		p.z = R * n.z * 0.5f * scale + C->z;
		CGameParticles_SetColorRampOn(pt, 1);
		pt->birthRate = 0;
		CGameParticles_SetColorRamp(pt, g_DevColorRamps + (crt_rand() & 1) * 9);
		pt->initPosMin = p;
		pt->initPosMax = p;
		pt->initVelMin = (vec3_t){ 0.0f, 0.0f, 0.0f };
		pt->initVelMax = (vec3_t){ 0.0f, 0.0f, 0.0f };
		pt->sizeIni = Dev_Rand(40.0, 220.0) * g_FXDevastatorScale * scale;
		pt->sizeEnd = Dev_Rand(40.0, 460.0) * g_FXDevastatorScale * scale;
		pt->spriteIni = 0x72;
		pt->spriteEnd = 0x72;
		pt->lifeTime = Dev_Rand(0x1.47ae1p-5, 0x1.70a3d8p-3);
		pt->sizeLife = Dev_Rand(0x1.47ae1p-5, 0x1.99999ap-3);
		pt->rotVar = 0.0f;
		float ang = Dev_Rand(0x1.921fb6p+2, 0.0);
		pt->rotIni = ang;
		pt->rotEnd = ang;
		CGameParticles_Apply(pt);
		CGameParticles_EmitN(pt, 1, 1);
		for (int i = 6; i; i--) {
			CGameParticles_SetColorRamp(pt, g_DevColorRamps + crt_rand() % 3 * 9);
			float a1 = Dev_RandSym(g_FXDevastatorRadius);
			float a2 = Dev_RandSym(g_FXDevastatorRadius);
			float a3 = Dev_RandSym(g_FXDevastatorRadius);
			pt->initPosMin.x = a3 * scale * 0.3f + p.x;
			pt->initPosMin.y = a2 * scale * 0.3f + p.y;
			pt->initPosMin.z = a1 * scale * 0.3f + p.z;
			pt->initPosMax = pt->initPosMin;
			pt->initVelMin = (vec3_t){ 0.0f, 0.0f, 0.0f };
			pt->initVelMax = (vec3_t){ 0.0f, 0.0f, 0.0f };
			pt->initAccMin = (vec3_t){ 0.0f, 0.0f, 0.0f };
			pt->initAccMax = (vec3_t){ 0.0f, 0.0f, 0.0f };
			pt->lifeTime = Dev_Rand(0x1.99999p-4, 0x1.19999ap-1);
			pt->sizeIni = Dev_Rand(100.0, 150.0) * g_FXDevastatorScale * scale;
			pt->sizeEnd = Dev_Rand(150.0, 450.0) * g_FXDevastatorScale * scale;
			pt->sizeLife = Dev_Rand(0x1.33333p-3, 0x1.333334p-1);
			pt->rotVar = 0.0f;
			ang = Dev_Rand(0x1.921fb6p+2, 0.0);
			pt->rotIni = ang;
			pt->rotEnd = ang;
			if (crt_rand() & 1) {
				pt->spriteIni = 0x31;
				pt->spriteEnd = 0x41;
			} else {
				pt->spriteIni = 0x41;
				pt->spriteEnd = 0x51;
			}
			CGameParticles_Apply(pt);
			CGameParticles_EmitDt(pt, 1, 1, Dev_Rand(0x1.47ae14p-6, 0x1.47ae14p-6));
		}

		/* the shot trail from the slot to the explosion */
		sys = cWorld_GetDebrisSys(g_pWorld, g_DevastatorShootFile);
		cDebris *dshot = cDebrisSys_NewElement(sys);
		dshot->speed = 0.05f;
		cDebrisSys_Drop(sys, dshot);
		cObjElementFXDevastatorShoot *sfx = Debris_Element(dshot);
		if (sfx) {
			CGameTrail *trail = &sfx->trail;
			if (!sfx->on) {
				sfx->on = 1;
			}
			sfx->duration = 0.075f;
			sfx->base.vftable->Reset(&sfx->base, dshot);
			CGameTrail_GetMaxTrails(trail);
			float sy = p.y - origin.y;
			float sx = p.x - origin.x;
			float sz = p.z - origin.z;
			float l = (float)__builtin_sqrt((double)(sy * sy + sx * sx + sz * sz));
			if (l != 0.0f) {
				float inv = 1.0f / l;
				u.x = inv * sx;
				u.y = inv * sy;
				u.z = inv * sz;
			} else {
				u.x = sx;
				u.y = sy;
				u.z = sz;
			}
			float m = 2500.0f;
			if (2500.0f > l) {
				m = l;
			}
			float mx = m * u.x;
			float my = m * u.y;
			float mz = m * u.z;
			q.x = mx * 0.68f + origin.x;
			q.z = mz * 0.68f + origin.z;
			q.y = my * 0.68f + origin.y;
			e.x = ((p.x - mx * 0.3f) - q.x) * 0.5f;
			e.y = ((p.y - my * 0.3f) - q.y) * 0.5f;
			e.z = ((p.z - mz * 0.3f) - q.z) * 0.5f;
			CGameTrail_Reset(trail);
			CGameTrail_Add(trail, origin, 250.0f, 0xff804110);
			CGameTrail_Add(trail, q, 250.0f, 0x60ff4110);
			CGameTrail_Add(trail, (vec3_t){ e.x + q.x, e.y + q.y, e.z + q.z }, 250.0f, 0x60ff4110);
			CGameTrail_Add(trail, (vec3_t){ p.x - mx * 0.28f, p.y - my * 0.28f, p.z - mz * 0.28f }, 250.0f,
				0x40ff4110);
			CGameTrail_Add(trail, p, 250.0f, 0x40ff5f20);
		}

		/* the sparks */
		sys = cWorld_GetDebrisSys(g_pWorld, g_DevastatorSparksFile);
		sys->radiusSq = 0.0f;
		cDebris *dsp = cDebrisSys_GetIdle(sys);
		cam = g_pWorld->camPos[g_pWorld->curCam];
		CGameModel_SetPos(Debris_Mod(dsp), &cam);
		dsp->speed = 1.0f;
		if (!cDebrisSys_IsIdle(sys, dsp)) {
			cDebrisSys_Drop(sys, dsp);
		}
		cObjElementFXSparks *spk = Debris_Element(dsp);
		if (!spk) {
			goto sphere;
		}
		if (!spk->on) {
			spk->on = 1;
		}
		float t2 = 1.0f - camDist / g_FXDevastatorDistance;
		float f2 = 1.0f;
		if (!(t2 > 1.0f)) {
			f2 = t2 * t2;
		}
		float f22 = f2 + f2;
		int ns = (int)(float)(Weap_Rand01() * (double)(f2 * 5.0f - f22) + (double)f22);
		if (ns <= 0) {
			goto sphere;
		}
		float sizeMin = scale * 75.0f;
		float sizeMax = scale * 125.0f;
		float jy = g_DebrisDefault * -4.0f;
		uint32_t color = (uint32_t)(crt_rand() % 60 + 0x78) << 8 | 0x10ff0000;
		cObjElementFXSparks_Emit(spk, ns, C->x, C->y, C->z, n.x, n.y, n.z, 0.942477822f, 0.942477822f, 0.942477822f,
			0.0f, jy, 0.0f, 1250.0f, 4500.0f, sizeMin, sizeMax, 1.0f, 0.0f, 0.35f, 0.55f, 0.15f, 0.2f, 0.0f, 0.05f,
			color, 550.0f, 1550.0f, 10.0f, 12.0f, 2);
	}
sphere:
	if (!(g_pApp->flags & APPF_CLIENT)) {
		cPy_Function_SetNative(g_InfernoSphereFunc, g_DevXplosionNative);
		int up = cWeaponManager_GetUpgrade(wm, g_DevastatorIDWeap);
		float rad = g_DevastatorExplosionRadius;
		if (up == 7) {
			rad = rad * 1.5f;
		}
		cWorld_SphereCallF(g_pWorld, 8, g_DevCenter, rad, g_InfernoSphereFunc, 1);
	}
	ATPC_PlaySound(g_DevastatorBoomSound, g_DevCenter, g_DevastatorBoomSoundAttIni, g_DevastatorBoomSoundAttEnd,
		g_DevastatorBoomSoundVol);
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
	cMissileEntity_SetSpeed((cMissileEntity *)emi, 0.0f);
	cMissileEntity_SetTurnSpeed((cMissileEntity *)emi, 0.0f);
	ammo = (int)fammo;
	cMissileEntity_SetCount((cMissileEntity *)emi, g_EMIAmmoNeed <= ammo ? g_EMIAmmoNeed : ammo);
	cPy_Function_AddParamS(g_SetUpEMIFunc, emi->name);
	cPy_Function_Execute(g_SetUpEMIFunc, 0);
	cMissileEntity_SetLifeTime((cMissileEntity *)emi, g_EMILifeTime - 0.2f);
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
		cMissileEntity_SetType((cMissileEntity *)e, 1);
		cMissileEntity_SetTarget((cMissileEntity *)e, GAME_STR(str_Empty, 0x3a3228));
	}
}

/* "!InfernoClientFire" (vehicle) */
void Inferno_ClientFireNet(cAbstractFile *params) {
	char name[128];

	cAbstractFile_ReadString(params, name, 0x80);
	Inferno_Shoot(name);
}
