/*
 * Entities/Vehicle/Weaps/Devastator.cpp of the game: the native functions of the Devastator (a spread shot that
 * explodes at the hit, "!DevastatorXplosionCall") and of the EMI, see include/scrap/weapons.h. Also the native
 * "!InfernoClientFire" that hands the name to Weaps/Inferno.cpp.
 */
#include "scrap/ai.h"
#include "scrap/fxmisc.h"
#include "scrap/mathlib.h"
#include "scrap/net.h"
#include "scrap/play.h"
#include "scrap/scorer.h"
#include "scrap/weapons.h"
#include "scrap/weaps2.h"
#include "scrap/staticinit.h"

#define g_DevastatorRechargeTime (GVar_DevastatorRechargeTime.value.f)
#define g_DevastatorExplosionRadius (GVar_DevastatorExplosionRadius.value.f)
#define g_DevastatorExtraDamage (GVar_DevastatorExtraDamage.value.f)
#define g_DevastatorHitForce (GVar_DevastatorHitForce.value.f)
#define g_DevastatorMinDamage (GVar_DevastatorMinDamage.value.f)
#define g_DevastatorSpreadAngle (GVar_DevastatorSpreadAngle.value.f)
#define g_DevastatorAmmoCost (GVar_DevastatorAmmoCost.value.f)
#define g_DevastatorBulletFile (GVar_DevastatorBulletFile.value.s)
#define g_DevastatorHitFile (GVar_DevastatorHitFile.value.s)
#define g_DevastatorShootFile (GVar_DevastatorShootFile.value.s)
#define g_DevastatorSparksFile (GVar_DevastatorSparksFile.value.s)
#define g_DevastatorBoomSound (GVar_DevastatorBoomSound.value.s)
#define g_DevastatorBoomSoundAttEnd (GVar_DevastatorBoomSoundAttEnd.value.f)
#define g_DevastatorBoomSoundAttIni (GVar_DevastatorBoomSoundAttIni.value.f)
#define g_DevastatorBoomSoundVol (GVar_DevastatorBoomSoundVol.value.f)
#define g_FXDevastatorDistance (GVar_FXDevastatorDistance.value.f)
#define g_FXDevastatorRadius (GVar_FXDevastatorRadius.value.f)
#define g_FXDevastatorScale (GVar_FXDevastatorScale.value.f)
#define g_DevXplosionNative (NativeFunc_DevastatorXplosionCall.func)	/* the function of "!DevastatorXplosionCall" */
#define g_CreateEMIFunc (((cPy_Function *)&GVar_CreateEMIFunc.func))
#define g_EMIRechargeTime (GVar_EMIRechargeTime.value.f)
#define g_EMIAmmoNeed (GVar_EMIAmmoNeed.value.i)
#define g_SetUpEMIFunc (((cPy_Function *)&GVar_SetUpEMIFunc.func))
#define g_EMILifeTime (GVar_EMILifeTime.value.f)
#define g_EMIIDAmmo (GVar_EMIIDAmmo.value.i)
static cPy_NativeFunc NativeFunc_DevastatorXplosionCall;
static cGVarFunction GVar_SetUpEMIFunc;
static cGVar GVar_FXDevastatorScale;
static cGVar GVar_FXDevastatorRadius;
static cGVar GVar_FXDevastatorDistance;
static cGVar GVar_EMIRechargeTime;
static cGVar GVar_EMILifeTime;
static cGVar GVar_EMIIDAmmo;
static cGVar GVar_EMIAmmoNeed;
static cGVar GVar_DevastatorSpreadAngle;
static cGVar GVar_DevastatorSparksFile;
static cGVar GVar_DevastatorShootFile;
static cGVar GVar_DevastatorRechargeTime;
static cGVar GVar_DevastatorMinDamage;
static cGVar GVar_DevastatorHitForce;
static cGVar GVar_DevastatorHitFile;
static cGVar GVar_DevastatorExtraDamage;
static cGVar GVar_DevastatorExplosionRadius;
static cGVar GVar_DevastatorBulletFile;
static cGVar GVar_DevastatorBoomSoundVol;
static cGVar GVar_DevastatorBoomSoundAttIni;
static cGVar GVar_DevastatorBoomSoundAttEnd;
static cGVar GVar_DevastatorBoomSound;
static cGVar GVar_DevastatorAmmoCost;
static cGVarFunction GVar_CreateEMIFunc;
#define DEV_PI 3.14159274f

/* the shot sounds 0xd..0xf of the vehicle loop with the recharge time of one slot */
static void Devastator_SetUp(cEntity *v);
static void Devastator_Start(cEntity *v) {
	if (!(cTargetLock_GetAmmo(&Veh(v)->weapons, g_DevastatorIDAmmo) > 0.0f)) {
		return;
	}
	Devastator_SetUp(v);
	int n = cWeaponManager_NumSlots(&Veh(v)->weapons, g_DevastatorIDWeap, 1);
	if (!n) {
		return;
	}
	if (Veh(v)->weapons.weapSnd == -1) {
		cWeaponManager_SetWeapSnd(&Veh(v)->weapons, 0xd);
		cSndItem *it = PtrArray_Get(&Veh(v)->wl.obj.snd, 0xd);
		if (it) {
			it->time = 0.0f;
		}
	}
	float t = g_DevastatorRechargeTime / (float)n;
	cWeaponManager_DelayFire(&Veh(v)->weapons, t);
	for (int i = 0xd; i <= 0xf; i++) {
		cSndItem *it = PtrArray_Get(&Veh(v)->wl.obj.snd, i);
		if (it) {
			it->times[0] = t;
		}
	}
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

/* Weaps/Devastator.cpp: the Devastator and the EMI */
static char g_DevShooter[128];
static char g_DevVictim[128];
static vec3_t g_DevCenter;

static uint32_t g_DevColorRamps[36] = {
	0x60ffffff, 0x60ffffff, 0x60ffffff, 0x60ffffff, 0x80ffffff, 0x80ffffff, 0xc0808080, 0xff000000, 0xff000000,
	0x80ffffc8, 0x80ffffc8, 0x80ffffc8, 0x80ffffc8, 0x80ffffc8, 0x80ffffc8, 0xc080805c, 0xff000000, 0xff000000,
	0x80ffc0c0, 0x80ffc0c0, 0x80ffc0c0, 0x80ffc0c0, 0x80ffc0c0, 0x80ffc0c0, 0xc0808080, 0xff000000, 0xff000000,
	0x80ffc080, 0x80ffc080, 0x80ffaf80, 0x80ffa480, 0x80ff9a6a, 0x80ff8c5c, 0xc0807840, 0xff000000, 0xff000000,
};

/* "!DevastatorCreate" (vehicle, hit point into (&g_DevCenter), entity hit, shot number): the shot of the next mounted
 * slot (a bullet for a player), then where the effects are visible the explosion particles on the floor at the hit,
 * the shot trail and the sparks, the damage of the sphere around the hit (not on a net client) and the boom */
static void Devastator_Create(cAbstractFile *params) {
	vec3_t c, r, in, rot, a, b, hit, n, p, origin, cam, d, u, q, e, mid;
	matrix_t mat;
	CCollisionTri tri;
	int shot;

	cAbstractFile_ReadString(params, g_DevShooter, 0x80);
	VFile_Read(params, &g_DevCenter.x, 4);
	VFile_Read(params, &g_DevCenter.y, 4);
	VFile_Read(params, &g_DevCenter.z, 4);
	cAbstractFile_ReadString(params, g_DevVictim, 0x80);
	VFile_Read(params, &shot, 4);
	cEntity *v = Weap_GetVehicle(g_DevShooter);
	cFollowCamEntity *playerCam = Entity_FollowCam(v);
	cWeaponManager *wm = &Veh(v)->weapons;
	int num = cWeaponManager_NumSlots(wm, g_DevastatorIDWeap, 1);
	if (!num) {
		return;
	}
	float q0 = (float)((cTargetLock_GetAmmo(wm, g_DevastatorIDAmmo)) / g_DevastatorAmmoCost);
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
		const matrix_t *m = CGameModel_NodeMatrix(Obj_GetModel(v), &mat, node, 0, NULL, 0);
		p.x = m->m[12];
		p.y = m->m[13];
		p.z = m->m[14];
		if (playerCam) {
			cDebrisSys *sys = cWorld_GetDebrisSys(g_pWorld, g_DevastatorBulletFile);
			cDebris *bullet = cDebrisSys_NewElement(sys);
			bullet->drop[0] = p;
			bullet->drop[1] = *Obj_GetViewAng(v, &rot);
			float rz = Dev_Rand(14.0, -7.0);
			float ry = Dev_Rand(2000.0, 0.0);
			float rx = Dev_Rand(200.0, -100.0);
			const vec3_t *pc = Obj_GetViewPos(v, &rot);
			float dx = (bullet->drop[0].x - pc->x) * 10.0f;
			float dy = (bullet->drop[0].y - pc->y) * 10.0f;
			float dz = (bullet->drop[0].z - pc->z) * 10.0f;
			const vec3_t *vel = Obj_GetVel(v, &c);
			bullet->drop[2].x = dx + vel->x * 0.5f + rx;
			bullet->drop[2].z = dz + vel->z * 0.5f + rz;
			bullet->drop[2].y = dy + vel->y * 0.5f + ry;
			float sz = Dev_Rand(14.0, -7.0);
			float sy = Dev_Rand(14.0, -7.0);
			bullet->drop[3].x = Dev_Rand(14.0, -7.0);
			bullet->drop[3].y = sy;
			bullet->drop[3].z = sz;
			bullet->life = 0.4f;
			cDebrisSys_Drop(sys, bullet);
		}
		cVehicleEffects_Shoot(Vehicle_Effects(v), g_DevastatorIDWeap, i);
		origin = p;
		break;
	}

	vec3_t *C = (&g_DevCenter);
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
			float rad = WLife(w)->prim.radius;
			const vec3_t *pc = Obj_GetViewPos(v, &c);
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
		dhit->life = 1.2f;
		if (!cDebrisSys_IsIdle(sys, dhit)) {
			cDebrisSys_Drop(sys, dhit);
		}
		cObjElementFXParticleSystem *fx = Debris_Element(dhit);
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
		const vec3_t *ro = Vec3_Rotate(&r, &in, Obj_GetViewAng(v, &rot));
		a.x = C->x + ro->x;
		a.z = ro->z + C->z;
		a.y = ro->y + C->y;
		in = (vec3_t){ 0.0f, 0.0f, -25.0f };
		ro = Vec3_Rotate(&r, &in, Obj_GetViewAng(v, &rot));
		b.z = ro->z + C->z;
		b.x = ro->x + C->x;
		b.y = ro->y + C->y;
		if (Engine_TestLine(&a, &b, &hit, &tri)) {
			n = tri.plane.n;
		} else {
			in = (vec3_t){ 0.0f, 0.0f, 1.0f };
			n = *Vec3_Rotate(&r, &in, Obj_GetViewAng(v, &rot));
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
			if (w && w == cWorld_GetUsrEntity(g_pWorld, 0) && Entity_IsType(w, TYEN_WITH_LIFE) && Entity_FollowCam(w)) {
				cFollowCamEntity *ws = Entity_FollowCam(w);
				scale = scale + scale;
				ws->quakeFactor = 0x1.015bf8p-5f;
				ws->quakeTime = 0.2f;
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
		dshot->life = 0.05f;
		cDebrisSys_Drop(sys, dshot);
		cObjElementFXDevastatorShoot *sfx = Debris_Element(dshot);
		if (sfx) {
			CGameTrail *trail = &sfx->trail;
			if (!sfx->on) {
				sfx->on = 1;
			}
			sfx->duration = 0.075f;
			FXObj_Reset(sfx, dshot);
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
		dsp->life = 1.0f;
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
		cWorld_SphereCallF(g_pWorld, 8, (&g_DevCenter), rad, g_InfernoSphereFunc, 1);
	}
	ATPC_PlaySound(g_DevastatorBoomSound, (&g_DevCenter), g_DevastatorBoomSoundAttIni, g_DevastatorBoomSoundAttEnd,
		g_DevastatorBoomSoundVol);
}

/* "!DevastatorFX" (vehicle, player, item): the HUD item turns while the vehicle fires (the upgrade 8 keeps turning),
 * the upgrade 7 pulses */
static void Devastator_FX(cAbstractFile *params) {
	char name[128], itemName[128];
	int player;

	cAbstractFile_ReadString(params, name, 0x80);
	VFile_Read(params, &player, 4);
	cAbstractFile_ReadString(params, itemName, 0x80);
	cEntity *v = Weap_GetVehicle(name);
	cCannonScorer *item = (cCannonScorer *)cScorer_FindItem(Scorer_Get(player), itemName);
	if (!item) {
		return;
	}
	float dt = Scorer_Get(player)->frameTime;
	int up = cWeaponManager_GetUpgrade(&Veh(v)->weapons, g_DevastatorIDWeap);
	if (Veh(v)->controls.fire && up == 8) {
		item->angle = dt * DEV_PI + item->angle;
	} else {
		float s = dt * DEV_PI;
		float a = NormAng(item->angle);
		item->angle = a;
		if (0.0f > a + s) {
			item->angle = a + s;
		} else if (a - s > 0.0f) {
			item->angle = a - s;
		} else {
			item->angle = 0.0f;
		}
	}
	if (Veh(v)->controls.fire && up == 7) {
		float time = Scorer_Get(player)->time;
		int k = (int)((double)time * 1024.0);
		int m = k % 0x1fe - 0xff;
		int ab = m < 0 ? -m : m;
		item->color = (((uint32_t)(ab / 4 - 0x41) << 8 | g_CannonScorerColorRed) << 8
			| g_CannonScorerColorGreen) << 8 | g_CannonScorerColorBlue;
		float size = (float)ab * 0.03125f + 20.0f;
		item->size[0] = size;
		item->size[1] = size;
		return;
	}
	item->size[0] = 20.0f;
	item->size[1] = 20.0f;
	item->color = Weap_CannonColor();
}

/* "!DevastatorFire" (vehicle): a shot spread at random around the aim, one life of the entity hit */
static void Devastator_Fire(cAbstractFile *params) {
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
	cWeaponManager *wm = &Veh(v)->weapons;
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
	ang[0] = Veh(v)->autoAim.ax + ry;
	ang[1] = Veh(v)->autoAim.ay + rx;
	Controls_RumbleA(&Veh(v)->controls, 0.7f, 0.2f);
	Controls_RumbleB(&Veh(v)->controls, 0.35f, 0.2f);
	dist = g_DevastatorRange;
	target = Vehicle_AimTarget(v);
	if (target) {
		/* the shot ends near the target of the aim help */
		const vec3_t *pa = Entity_GetPos(v, &a);
		const vec3_t *pb = Entity_GetPos(target, &b);
		float dx = pb->x - pa->x;
		float dy = pb->y - pa->y;
		float dz = pb->z - pa->z;
		float d = (float)__builtin_sqrt((double)(dy * dy + dx * dx + dz * dz));
		if (dist > d) {
			float r = g_DevastatorExplosionRadius;
			dist = (float)(Weap_Rand01() * (double)r + 0.0) + d;
		}
	}
	Rep_RewindBegin(v);
	Entity_GetTargetAngle(v, ang, dist, &target);
	Entity_AimWeapon(v, ang[0], ang[1], &dist, &hit, &target, 8);
	Rep_RewindEnd(v);
	if (target) {
		if (Entity_IsType(target, TYEN_WITH_LIFE) && !WLife(target)->god && Rep_Hurt(v, target, "Devastator")) {
			WLife(target)->life--;
		}
		if (target && Entity_IsType(target, TYEN_VEHICLE)) {
			cVehicleAttack_ShotAt(&WLife(v)->attack, target->name, 1);
			goto ammo;
		}
	}
	cVehicleAttack_ShotAtPoint(&WLife(v)->attack, &hit, 1);
ammo:;
	float rest = (float)((cTargetLock_GetAmmo(wm, g_DevastatorIDAmmo)) - g_DevastatorAmmoCost);
	cWeaponManager_SetAmmo(wm, g_DevastatorIDAmmo, Float_MaxSS(0.0f, rest));
	/* the upgrades 7 and 8 use up their own ammo, at 0 the script restores the weapon */
	int up = cWeaponManager_GetUpgrade(wm, g_DevastatorIDWeap);
	if ((uint32_t)(up - 7) > 1) {
		return;
	}
	float u = (float)((cWeaponManager_GetUAmmo(wm, g_DevastatorIDWeap)) - 2.0f);
	if (0.0f >= u) {
		cWeaponManager_SetUpgrade(wm, g_DevastatorIDWeap, 0);
		u = 0.0f;
		cPy_Function_AddParamI(g_RestoreUpgradeFunc, up);
		cPy_Function_Execute(g_RestoreUpgradeFunc, 0);
	}
	cWeaponManager_SetUAmmo(wm, g_DevastatorIDWeap, u);
}

/* "!DevastatorPress" (vehicle) */
static void Devastator_Press(cAbstractFile *params) {
	char name[128];

	cAbstractFile_ReadString(params, name, 0x80);
	cEntity *v = Weap_GetVehicle(name);
	Devastator_SetUp(v);
}

/* "!DevastatorRelease" (vehicle) */
static void Devastator_Release(cAbstractFile *params) {
	char name[128];

	cAbstractFile_ReadString(params, name, 0x80);
	cEntity *v = Weap_GetVehicle(name);
	Veh(v)->autoAim.status = 0;
}

/* the aim help of the Devastator, wider for a pad with VehicleCenterPadLock */
static void Devastator_SetUp(cEntity *v) {
	Veh(v)->autoAim.status = 1;
	float ang;
	if (g_VehicleCenterPadLock != 0.0f && cVehicleEntity_GetOwnerSlot(v) != -1) {
		ang = 0.34906584f;
	} else {
		ang = 0.2443461f;
	}
	Veh(v)->autoAim.speed = 0.75049156f;
	cAutoAim_SetAngle(&Veh(v)->autoAim, ang);
	Veh(v)->autoAim.distLimit = g_DevastatorDistLimit;
	cAutoAim_Update(&Veh(v)->autoAim, v, 0.025f);
}

/* "!DevastatorXplosionCall" (entity): the damage and push of the explosion at (&g_DevCenter) */
static void Devastator_XplosionCall(cAbstractFile *params) {
	vec3_t p, in, r, n;

	cAbstractFile_ReadString(params, g_DevVictim, 0x80);
	cEntity *victim = World_FindEntity(g_DevVictim);
	if (!victim || !Entity_IsType(victim, TYEN_WITH_LIFE)) {
		return;
	}
	int oldLife = WLife(victim)->life;
	const vec3_t *pp = Entity_GetPos(victim, &p);
	float dy = pp->y - g_DevCenter.y;
	float dx = pp->x - g_DevCenter.x;
	float dz = pp->z - g_DevCenter.z;
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
	float f = (rad - Float_MaxSS(1.0f, d - WLife(victim)->prim.radius)) / rad;
	int dmg = (int)(g_DevastatorExtraDamage * f + g_DevastatorMinDamage);
	if (World_IsEasySingle()) {
		cEntity *shooter = World_FindEntity(g_DevShooter);
		if (shooter && Entity_GetAI(shooter)) {
			dmg = dmg / 2;
			if (dmg <= 0) {
				dmg = 1;
			}
		}
	}
	if (!WLife(victim)->god && Rep_Hurt(World_FindEntity(g_DevShooter), victim, "Devastator")) {
		WLife(victim)->life -= dmg;
	}
	if (oldLife > WLife(victim)->life) {
		cVehicleAttack_HitBy(&WLife(victim)->attack, g_DevShooter);
	}
	if (!Entity_IsType(victim, TYEN_WALKCHAR)) {
		float x = dx * f;
		float y = dy * f;
		float z = dz * f;
		float k = g_DevastatorHitForce * 1000.0f / WLife(victim)->mass;
		x = x * k;
		y = y * k;
		z = z * k;
		const vec3_t *vel = Obj_GetVel(victim, &r);
		n.x = x + vel->x;
		n.y = vel->y + y;
		n.z = vel->z + z;
		Obj_SetVel(victim, &n);
	}
	if (Entity_IsType(victim, TYEN_VEHICLE) && Veh(victim)->netCtrl) {
		Veh(victim)->netCtrl->hit = 2;
	}
}

/* "!EMIFire" (vehicle): the script makes the EMI with the ammo of the vehicle */
static void EMI_Fire(cAbstractFile *params) {
	char name[128], emiName[128];
	vec3_t pos, ang;

	cAbstractFile_ReadString(params, name, 0x80);
	cEntity *v = World_FindEntity(name);
	if (!v || !Entity_IsType(v, TYEN_VEHICLE)) {
		return;
	}
	cWeaponManager *wm = &Veh(v)->weapons;
	float fammo = cTargetLock_GetAmmo(wm, g_EMIIDAmmo);
	int ammo = (int)fammo;
	if (WithLife_IsEMIed(v) || ammo < 1) {
		return;
	}
	Veh(v)->weapons.cmStamp = -g_EMIRechargeTime;
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
	Net_GetObjName(emiName);
	Entity_GetPos(v, &pos);
	Obj_GetAng(v, &ang);
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
	Controls_RumbleA(&Veh(v)->controls, 0.4f, 0.2f);
	cMissileEntity_SetSpeed((cMissileEntity *)emi, 0.0f);
	cMissileEntity_SetTurnSpeed((cMissileEntity *)emi, 0.0f);
	ammo = (int)fammo;
	cMissileEntity_SetCount((cMissileEntity *)emi, g_EMIAmmoNeed <= ammo ? g_EMIAmmoNeed : ammo);
	cPy_Function_AddParamS(g_SetUpEMIFunc, emi->name);
	cPy_Function_Execute(g_SetUpEMIFunc, 0);
	cMissileEntity_SetLifeTime((cMissileEntity *)emi, g_EMILifeTime - 0.2f);
	cPy_Function_SetFunc(&Missile(emi)->onHit, "!EMIOnHit");
	cWeaponManager_SetAmmo(wm, g_EMIIDAmmo, (float)(ammo - Missile(emi)->swarmCount));
}

/* "!EMIOnHit" (EMI, ?, entity): a vehicle near the EMI is out for 10 seconds, further away for a random time (or not
 * at all); a missile in the radius loses its target */
static void EMI_OnHit(cAbstractFile *params) {
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
	const vec3_t *pa = Entity_GetPos(e, &a);
	const vec3_t *pb = Entity_GetPos(emi, &b);
	float dx = pb->x - pa->x;
	float dy = pb->y - pa->y;
	float dz = pb->z - pa->z;
	float d = (float)__builtin_sqrt((double)(dy * dy + dx * dx + dz * dz));
	float r = Missile(emi)->radius;
	if (Entity_IsType(e, TYEN_VEHICLE)) {
		if (g_EMILowRadiusFactor * r >= d) {
			Veh(e)->emi = 10.0f;
			return;
		}
		float t = Veh(e)->emi;
		if (1.0f > t) {
			if (crt_rand() & 1) {
				Veh(e)->emi = 0.0f;
				return;
			}
			Veh(e)->emi = Rand_FloatRange(0.0f, d / r);
			return;
		}
		if (t > 5.0f) {
			Veh(e)->emi = 5.0f;
		}
		return;
	}
	if (Entity_IsType(e, "Missile") && r >= d) {
		cMissileEntity_SetType((cMissileEntity *)e, 1);
		cMissileEntity_SetTarget((cMissileEntity *)e, str_Empty);
	}
}

/* "!InfernoClientFire" (vehicle) */
static void Inferno_ClientFireNet(cAbstractFile *params) {
	char name[128];

	cAbstractFile_ReadString(params, name, 0x80);
	Inferno_Shoot(name);
}

cGVar GVar_DevastatorIDWeap;
static cGVar GVar_DevastatorBulletFile;
static cGVar GVar_FXDevastatorDistance;
static cGVar GVar_DevastatorRechargeTime;
static cGVar GVar_DevastatorExplosionRadius;
static cGVar GVar_DevastatorExtraDamage;
static cGVar GVar_DevastatorHitForce;
cGVar GVar_DevastatorIDAmmo;
static cPy_NativeFunc NativeFunc_DevastatorXplosionCall;
static cGVar GVar_DevastatorShootFile;
static cGVar GVar_DevastatorBoomSoundVol;
static cGVar GVar_DevastatorMinDamage;
static cGVar GVar_DevastatorSpreadAngle;
static cGVar GVar_FXDevastatorRadius;
cGVar GVar_DevastatorRange;
static cGVar GVar_DevastatorSparksFile;
static cGVar GVar_DevastatorBoomSoundAttEnd;
static cGVar GVar_DevastatorHitFile;
static cGVar GVar_DevastatorBoomSound;
static cGVar GVar_FXDevastatorScale;
static cGVar GVar_DevastatorAmmoCost;
static cGVar GVar_DevastatorBoomSoundAttIni;
static cGVarFunction GVar_CreateEMIFunc;
static cGVar GVar_EMIRechargeTime;
static cGVar GVar_EMIAmmoNeed;
static cGVarFunction GVar_SetUpEMIFunc;
static cGVar GVar_EMILifeTime;
static cGVar GVar_EMIIDAmmo;
static cPy_NativeFunc NativeFunc_DevastatorPress;
static cPy_NativeFunc NativeFunc_DevastatorRelease;
static cPy_NativeFunc NativeFunc_DevastatorCreate;
static cPy_NativeFunc NativeFunc_DevastatorFire;
static cPy_NativeFunc NativeFunc_DevastatorFX;
static cPy_NativeFunc NativeFunc_EMIOnHit;
static cPy_NativeFunc NativeFunc_EMIFire;
static cPy_NativeFunc NativeFunc_InfernoClientFire;

/* the dynamic initializers of the globals here, run in the order of src/standalone/crtxc.inc */
INIT_GVAR_INT(Init_GVar_DevastatorIDWeap, &GVar_DevastatorIDWeap, "DevastatorIDWeap", 0, "The default weapon idx for the Devastator", 3)
INIT_GVAR_INT(Init_GVar_DevastatorIDAmmo, &GVar_DevastatorIDAmmo, "DevastatorIDAmmo", 0, "The default weapon idx for the Devastator", 0)
FINI_GVAR_STRING(Fini_GVar_DevastatorBulletFile, &GVar_DevastatorBulletFile)
INIT_GVAR_STRING(Init_GVar_DevastatorBulletFile, Fini_GVar_DevastatorBulletFile, &GVar_DevastatorBulletFile, "DevastatorBulletFile", 0, "default bullet model for the Devastator", "Models/Weapons/Debris/Bullet/Bullet.M3D")
FINI_GVAR_STRING(Fini_GVar_DevastatorHitFile, &GVar_DevastatorHitFile)
INIT_GVAR_STRING(Init_GVar_DevastatorHitFile, Fini_GVar_DevastatorHitFile, &GVar_DevastatorHitFile, "DevastatorHitFile", 0, "default hit model for the Devastator", "Models/Weapons/Devastator/Hit.M3D")
FINI_GVAR_STRING(Fini_GVar_DevastatorShootFile, &GVar_DevastatorShootFile)
INIT_GVAR_STRING(Init_GVar_DevastatorShootFile, Fini_GVar_DevastatorShootFile, &GVar_DevastatorShootFile, "DevastatorShootFile", 0, "default shoot model for the Devastator", "Models/Weapons/Devastator/Shoot.M3D")
FINI_GVAR_STRING(Fini_GVar_DevastatorSparksFile, &GVar_DevastatorSparksFile)
INIT_GVAR_STRING(Init_GVar_DevastatorSparksFile, Fini_GVar_DevastatorSparksFile, &GVar_DevastatorSparksFile, "DevastatorSparksFile", 0, "default sparks model for the Devastator", "Models/Weapons/Devastator/Sparks.M3D")
FINI_GVAR_STRING(Fini_GVar_DevastatorBoomSound, &GVar_DevastatorBoomSound)
INIT_GVAR_STRING(Init_GVar_DevastatorBoomSound, Fini_GVar_DevastatorBoomSound, &GVar_DevastatorBoomSound, "DevastatorBoomSound", 0, "Explosion sound for devastator", "DevastatorBoom")
INIT_GVAR_FLOAT(Init_GVar_DevastatorBoomSoundVol, &GVar_DevastatorBoomSoundVol, "DevastatorBoomSoundVol", 0, "Explosion sound volume for devastator", 1.0f)
INIT_GVAR_FLOAT(Init_GVar_DevastatorBoomSoundAttIni, &GVar_DevastatorBoomSoundAttIni, "DevastatorBoomSoundAttIni", 0, "Explosion sound attenuation ini for devastator", 0.0f)
INIT_GVAR_FLOAT(Init_GVar_DevastatorBoomSoundAttEnd, &GVar_DevastatorBoomSoundAttEnd, "DevastatorBoomSoundAttEnd", 0, "Explosion sound attenuation end for devastator", 0.0f)
INIT_GVAR_FLOAT(Init_GVar_DevastatorRange, &GVar_DevastatorRange, "DevastatorRange", 0, "The maximun range that have the Devastator", 40000.0f)
INIT_GVAR_FLOAT(Init_GVar_DevastatorAmmoCost, &GVar_DevastatorAmmoCost, "DevastatorAmmoCost", 0, "Cost Per hit at Devastator", 1.0f)
INIT_GVAR_FLOAT(Init_GVar_DevastatorSpreadAngle, &GVar_DevastatorSpreadAngle, "DevastatorSpreadAngle", 0, "angle of random values used by Devastator", 4.0f)
INIT_GVAR_FLOAT(Init_GVar_DevastatorExplosionRadius, &GVar_DevastatorExplosionRadius, "DevastatorExplosionRadius", 0, "Explosion radius by a Single Devastator shoot", 500.0f)
INIT_GVAR_FLOAT(Init_GVar_DevastatorHitForce, &GVar_DevastatorHitForce, "DevastatorHitForce", 0, "The force that have a single Devastator shoot cm*TN/sec", 12000.0f)
INIT_GVAR_FLOAT(Init_GVar_DevastatorRechargeTime, &GVar_DevastatorRechargeTime, "DevastatorRechargeTime", 0, "Time between a single vulcan shoot.", 0.25f)
INIT_GVAR_FLOAT(Init_GVar_DevastatorMinDamage, &GVar_DevastatorMinDamage, "DevastatorMinDamage", 0, "The base of the minimal damage of Devastator", 2.0f)
INIT_GVAR_FLOAT(Init_GVar_DevastatorExtraDamage, &GVar_DevastatorExtraDamage, "DevastatorExtraDamage", 0, "Aditional damage about the distance", 5.0f)
INIT_GVAR_FLOAT(Init_GVar_FXDevastatorDistance, &GVar_FXDevastatorDistance, "FXDevastatorDistance", 0, "Distance of Devastator Explosion Effect", 95000.0f)
INIT_GVAR_FLOAT(Init_GVar_FXDevastatorRadius, &GVar_FXDevastatorRadius, "FXDevastatorRadius", 0, "Radius of Devastator Explosion Effect", 600.0f)
INIT_GVAR_FLOAT(Init_GVar_FXDevastatorScale, &GVar_FXDevastatorScale, "FXDevastatorScale", 0, "Scale of Devastator Explosion Effect", 1.2f)
INIT_NATIVE_FUNC(Init_NativeFunc_DevastatorXplosionCall, &NativeFunc_DevastatorXplosionCall, "DevastatorXplosionCall", Devastator_XplosionCall)
FINI_CALL(Fini_GVar_CreateEMIFunc, &GVar_CreateEMIFunc.func, cPy_Function_dtor)
INIT_GVAR_FUNCTION(Init_GVar_CreateEMIFunc, Fini_GVar_CreateEMIFunc, &GVar_CreateEMIFunc, "CreateEMIFunc", 0, "Function to be called to create the EMI", "")
FINI_CALL(Fini_GVar_SetUpEMIFunc, &GVar_SetUpEMIFunc.func, cPy_Function_dtor)
INIT_GVAR_FUNCTION(Init_GVar_SetUpEMIFunc, Fini_GVar_SetUpEMIFunc, &GVar_SetUpEMIFunc, "SetUpEMIFunc", 0, "Function to be called to set up the EMI properties", "")
INIT_GVAR_INT(Init_GVar_EMIIDAmmo, &GVar_EMIIDAmmo, "EMIIDAmmo", 0, "EMI Ammo ID", 0)
INIT_GVAR_INT(Init_GVar_EMIAmmoNeed, &GVar_EMIAmmoNeed, "EMIAmmoNeed", 0, "The deault cost ammunition for the EMI", 5)
INIT_GVAR_FLOAT(Init_GVar_EMIRechargeTime, &GVar_EMIRechargeTime, "EMIRechargeTime", 0, "EMI Recharge Time", 1.5f)
INIT_GVAR_FLOAT(Init_GVar_EMILifeTime, &GVar_EMILifeTime, "EMILifeTime", 0, "EMI Life Time", 10.0f)
INIT_NATIVE_FUNC(Init_NativeFunc_DevastatorPress, &NativeFunc_DevastatorPress, "DevastatorPress", Devastator_Press)
INIT_NATIVE_FUNC(Init_NativeFunc_DevastatorRelease, &NativeFunc_DevastatorRelease, "DevastatorRelease", Devastator_Release)
INIT_NATIVE_FUNC(Init_NativeFunc_DevastatorCreate, &NativeFunc_DevastatorCreate, "DevastatorCreate", Devastator_Create)
INIT_NATIVE_FUNC(Init_NativeFunc_DevastatorFire, &NativeFunc_DevastatorFire, "DevastatorFire", Devastator_Fire)
INIT_NATIVE_FUNC(Init_NativeFunc_DevastatorFX, &NativeFunc_DevastatorFX, "DevastatorFX", Devastator_FX)
INIT_NATIVE_FUNC(Init_NativeFunc_EMIOnHit, &NativeFunc_EMIOnHit, "EMIOnHit", EMI_OnHit)
INIT_NATIVE_FUNC(Init_NativeFunc_EMIFire, &NativeFunc_EMIFire, "EMIFire", EMI_Fire)
INIT_NATIVE_FUNC(Init_NativeFunc_InfernoClientFire, &NativeFunc_InfernoClientFire, "InfernoClientFire", Inferno_ClientFireNet)
