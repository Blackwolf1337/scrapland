/*
 * Entities/Vehicle/Weaps/Vulcan.cpp of the game: the Vulcan, the machine gun of the ships (VulcanDamage per mounted
 * bay and shot), see include/scrap/weaps2.h. Its upgrades 1 (faster) and 2 (wider aim, the HUD item spins) use up
 * their own ammo.
 */
#include "scrap/vehicleentity.h"
#include "scrap/weaps2.h"
#include "scrap/staticinit.h"

#define VULCAN_UPG_FAST 1
#define VULCAN_UPG_WIDE 2
#define VULCAN_SND_SHOT 5		/* the shot sounds 5..7 of the ship */


/* a random float of the effects: rand() in [0, 1] times mul plus add, in double */
static inline float Vul_Rand(double mul, double add) {
	return (float)(Weap_Rand01() * mul + add);
}

/* the empty shell of every mounted bay (only for the ship of a player) and the shoot effect */
static void Vulcan_Shells(cEntity *v) {
	vec3_t ang, ctr, vel;
	matrix_t mtx;

	cEntity *player = Obj(v)->cam;
	for (int i = 0; i < 4; i++) {
		if (!cTargetLock_HasSlot(&Veh(v)->weapons, g_VulcanIDWeap, i)) {
			continue;
		}
		const matrix_t *m = CGameModel_NodeMatrix(Obj_Vtbl(v)->GetModel(v), &mtx,
			Veh(v)->weapons.nodes[g_VulcanIDAmmo][i], 0, NULL, 0);
		float sx = m->m[12];
		float sy = m->m[13];
		float sz = m->m[14];
		if (player) {
			cDebrisSys *sys = cWorld_GetDebrisSys(g_pWorld, g_VulcanBulletFile);
			cDebris *d = cDebrisSys_NewElement(sys);
			d->drop[0].x = sx;
			d->drop[0].y = sy;
			d->drop[0].z = sz;
			d->drop[1] = *Obj_Vtbl(v)->GetViewAng(v, &ang);
			float rz = Vul_Rand(14.0, -7.0);
			float ry = Vul_Rand(2000.0, 0.0);
			float rx = Vul_Rand(200.0, -100.0);
			const vec3_t *c = Obj_Vtbl(v)->GetViewPos(v, &ctr);
			float dx = (d->drop[0].x - c->x) * 10.0f;
			float dy = (d->drop[0].y - c->y) * 10.0f;
			float dz = (d->drop[0].z - c->z) * 10.0f;
			const vec3_t *pv = Obj_Vtbl(v)->GetVel(v, &vel);
			float vx = dx + pv->x * 0.5f;
			float vy = dy + pv->y * 0.5f;
			float vz = dz + pv->z * 0.5f;
			d->drop[2].x = rx + vx;
			d->drop[2].y = ry + vy;
			d->drop[2].z = rz + vz;
			float az = Vul_Rand(14.0, -7.0);
			float ay = Vul_Rand(14.0, -7.0);
			d->drop[3].x = Vul_Rand(14.0, -7.0);
			d->drop[3].y = ay;
			d->drop[3].z = az;
			d->life = 0.4f;
			cDebrisSys_Drop(sys, d);
		}
		cVehicleEffects_Shoot(Veh_Effects(v), g_VulcanIDWeap, i);
	}
}

/* the start position of the particles of the next emit; the velocity and acceleration 0 */
static void Vul_PosOnly(CGameParticles *p, const vec3_t *off) {
	p->initPosMin = *off;
	p->initPosMax = *off;
	p->initVelMin.x = 0.0f;
	p->initVelMin.y = 0.0f;
	p->initVelMin.z = 0.0f;
	p->initVelMax.x = 0.0f;
	p->initVelMax.y = 0.0f;
	p->initVelMax.z = 0.0f;
	p->initAccMin.x = 0.0f;
	p->initAccMin.y = 0.0f;
	p->initAccMin.z = 0.0f;
	p->initAccMax.x = 0.0f;
	p->initAccMax.y = 0.0f;
	p->initAccMax.z = 0.0f;
}

uint32_t g_VulcanRamps[3][9] = {
	{ 0x80fff696, 0x20ffb45a, 0x40e68211, 0x80ab5e0d, 0x964c4408, 0xa4380f03, 0xc0280700, 0xff190200, 0xff000000 },
	{ 0x80ffebd2, 0x20ffd75a, 0x40e6c811, 0x80ab8c0d, 0x964c3a08, 0xa4380f03, 0xc0280700, 0xff190200, 0xff000000 },
	{ 0xff808080, 0x80808080, 0x5c808080, 0x80808080, 0xa4808080, 0xc0808080, 0xde808080, 0xff000000, 0xff000000 },
};

/* the sparks of a hit: fewer far from the camera, more on a target */
static void Vul_Sparks(CGameParticles *p, const matrix_t *mtx, float dist, float size, float k, byte target) {
	vec3_t dir;

	int n = -(crt_rand() & 1) - (int)((g_FXVulcanHitDistance - dist) / g_FXVulcanHitDistance * -15.0f);
	if (target) {
		n = (int)((float)n * 1.5f);
	}
	for (; n > 0; n--) {
		int sign = crt_rand();
		CGameParticles_SetColorRamp(p, g_VulcanRamps[crt_rand() & 1]);
		float vy = Vul_Rand(250.0, 50.0);
		if (!(sign & 2)) {
			vy = -vy;
		}
		float vx = Vul_Rand(250.0, 50.0);
		if (!(sign & 1)) {
			vx = -vx;
		}
		p->initVelMin.x = vx * size;
		p->initVelMin.y = vy * size;
		p->initVelMin.z = Vul_Rand(4150.0, 350.0) * size;
		p->initVelMax = p->initVelMin;
		float spd = Vul_Rand(180.0, 35.0);
		Weap_Norm(&dir, p->initVelMin.x, p->initVelMin.y, p->initVelMin.z);
		p->initPosMin.x = spd * dir.x;
		p->initPosMin.y = spd * dir.y;
		p->initPosMin.z = spd * dir.z;
		p->initPosMax = p->initPosMin;
		float acc = Vul_Rand(0.10000002384185791, 0.6499999761581421);
		float ax = -p->initVelMin.x;
		float ay = -p->initVelMin.y;
		float az = -p->initVelMin.z;
		p->initAccMin.x = acc * ax;
		p->initAccMin.y = acc * ay;
		p->initAccMin.z = acc * az;
		p->initAccMax = p->initAccMin;
		float dt = Vul_Rand(0.019999999552965164, 0.0);
		float life = Vul_Rand(0.19999998807907104, 0.25);
		CGameParticles_SetSparksOn(p, 1);
		p->lifeTime = life;
		float sz = Vul_Rand(50.0, 50.0) * size;
		p->sizeIni = sz;
		p->sizeEnd = sz;
		p->sizeLife = 1.0f;
		p->rotIni = 0.0f;
		p->rotEnd = 0.0f;
		p->rotVar = 0.0f;
		int sprite = crt_rand() % 3;
		p->spriteEnd = 0x44;
		p->sparkVelScale = 1.0f;
		p->spriteIni = (byte)(sprite + 0x41);
		p->sparkMaxSize = Vul_Rand(1.6999999284744263, 0.4000000059604645) * k;
		CGameParticles_Apply(p);
		CGameParticles_Emit(p, 1, 1, dt + 0x1.eb851ep-6f, mtx);
	}
	CGameParticles_SetSparksOn(p, 0);
}

/* the dust of a hit on the floor */
static void Vul_Dust(CGameParticles *p, const matrix_t *mtx, const vec3_t *off, float size) {
	CGameParticles_SetColorRamp(p, g_VulcanRamps[2]);
	CGameParticles_SetSparksOn(p, 0);
	p->initPosMin = *off;
	p->initPosMax = *off;
	float z = Vul_Rand(250.0, 500.0);
	float y = Vul_Rand(100.0, -50.0);
	p->initVelMin.y = y * size;
	p->initVelMin.z = z * size;
	p->initVelMin.x = Vul_Rand(100.0, -50.0) * size;
	p->initVelMax = p->initVelMin;
	float ax = -p->initVelMin.x;
	float ay = -p->initVelMin.y;
	float az = -p->initVelMin.z;
	p->initAccMin.x = ax * 0.2f;
	p->initAccMin.y = ay * 0.2f;
	p->initAccMin.z = az * 0.2f;
	p->initAccMax = p->initAccMin;
	p->sizeIni = Vul_Rand(20.0, 145.0) * size;
	p->sizeEnd = Vul_Rand(35.0, 290.0) * size;
	float life = Vul_Rand(0.050000011920928955, 0.75);
	p->sizeLife = 1.0f;
	p->lifeTime = life;
	if (crt_rand() & 1) {
		byte sprite = (byte)((crt_rand() & 1) + 0x11);
		p->spriteIni = sprite;
		p->spriteEnd = sprite;
	} else {
		p->spriteIni = 0x21;
		p->spriteEnd = 0x25;
	}
	float rot = Vul_Rand(6.2831854820251465, 0.0);
	p->rotIni = rot;
	p->rotEnd = rot;
	p->rotVar = Vul_Rand(0.7853981852531433, -0.39269909262657166);
	CGameParticles_Apply(p);
	CGameParticles_EmitM(p, 1, 1, mtx);
}

/* one puff of the hit: the size and life ramps */
static void Vul_Puff(CGameParticles *p, float size, double ini, double iniAdd, double end,
	double endAdd, float sizeLife, uint16_t sprite, double life, double lifeAdd) {
	p->sizeIni = Vul_Rand(ini, iniAdd) * size;
	p->sizeEnd = Vul_Rand(end, endAdd) * size;
	float t = Vul_Rand(life, lifeAdd);
	p->sizeLife = sizeLife;
	p->spriteIni = (byte)sprite;
	p->spriteEnd = (byte)(sprite >> 8);
	p->lifeTime = t;
}

/* the reload of a shot (with ammo): the recoil, the sound times and the delay of the next shot */
void Vulcan_Shoot(cEntity *v) {
	cTargetLock *wm = &Veh(v)->weapons;
	float ammo = cTargetLock_GetAmmo(wm, g_VulcanIDAmmo);
	if (!(ammo > 0.0f)) {
		return;
	}
	Controls_RumbleA(&Veh(v)->controls, 0.5f, 0.2f);
	Controls_RumbleB(&Veh(v)->controls, 0.35f, 0.2f);
	if (Veh(v)->weapons.weapSnd == -1) {
		cWeaponManager_SetWeapSnd(wm, VULCAN_SND_SHOT);
		cSndItem *s = PtrArray_Get(&Obj(v)->snd, VULCAN_SND_SHOT);
		if (s) {
			s->time = 0.0f;
		}
	}
	/* without the sound the original keeps the ammo in the variable of the time */
	float t = ammo;
	cSndItem *s;
	if (cWeaponManager_GetUpgrade(wm, g_VulcanIDWeap) == VULCAN_UPG_FAST) {
		cWeaponManager_DelayFire(wm, g_VulcanTimeDelayUPG);
		s = PtrArray_Get(&Obj(v)->snd, VULCAN_SND_SHOT);
		if (s) {
			t = s->times[2];
		}
	} else {
		cWeaponManager_DelayFire(wm, g_VulcanTimeDelay);
		s = PtrArray_Get(&Obj(v)->snd, VULCAN_SND_SHOT);
		if (s) {
			t = s->times[1];
		}
	}
	for (int i = VULCAN_SND_SHOT; i < VULCAN_SND_SHOT + 3; i++) {
		s = PtrArray_Get(&Obj(v)->snd, i);
		if (s) {
			s->times[0] = t;
		}
	}
}

/* the ricochet sound of a shot into the void or the floor */
static void Vul_Ricochet(const vec3_t *hit) {
	cSoundDesc desc;
	const char *name;
	int vol;

	cSoundDesc_Init(&desc);
	desc.param1 = g_VulcanRicSoundAttIni;
	desc.param2 = g_VulcanRicSoundAttEnd;
	desc.param3 = 0.0f;
	cSoundDesc_SetPos(&desc, hit);
	switch (crt_rand() % 3) {
	case 0:
		vol = Snd_Volume(g_VulcanRic0SoundVol);
		name = g_VulcanRic0Sound;
		break;
	case 1:
		vol = Snd_Volume(g_VulcanRic1SoundVol);
		name = g_VulcanRic1Sound;
		break;
	default:
		vol = Snd_Volume(g_VulcanRic2SoundVol);
		name = g_VulcanRic2Sound;
		break;
	}
	desc.volume += vol;
	g_pSound->vftable->PlaySound(g_pSound, name, &desc, NULL);
}

/*
 * ship name, hit point x y z, target name: the shells and the shoot effect of every mounted bay, then (when the hit is
 * seen) the hit particles (on the surface of the target, or along the normal of the floor with VulcanHitUseNormal),
 * the hit sound and a push of the target, or the ricochet sound
 */
void Vulcan_CreateNative(cAbstractFile *f) {
	char name[0x80], targetName[0x80];
	vec3_t hit, tmp, n, ang, from, to, out, cam, off;
	CCollisionTri tri;
	matrix_t mtx;

	cAbstractFile_ReadString(f, name, 0x80);
	Weap_ReadVec(f, &hit);
	cAbstractFile_ReadString(f, targetName, 0x80);
	cEntity *v = Weap_Vehicle(name);
	cEntity *player = Obj(v)->cam;
	Vulcan_Shells(v);
	if (!FX_IsVisible(hit.x, hit.y, hit.z, 500.0f, g_FXVulcanHitDistance, NULL, 0)) {
		goto target;
	}
	if (*targetName) {
		cEntity *w = World_FindEntity(targetName);
		CHECK(w);
		CHECK(Entity_IsType(w, TYEN_WITH_LIFE));
		float rad = WLife(w)->prim.radius;
		const vec3_t *c = Obj_Vtbl(v)->GetViewPos(v, &tmp);
		Weap_Norm(&n, c->x - hit.x, c->y - hit.y, c->z - hit.z);
		float rr = Vul_Rand(0.30000001192092896, 0.30000001192092896);
		hit.z = rr * (n.z * rad) + hit.z;
		hit.x = rr * (n.x * rad) + hit.x;
		hit.y = rr * (n.y * rad) + hit.y;
	}
	cWorld *wo = g_pWorld;
	cam = wo->camPos[wo->curCam];
	float cx = cam.x - hit.x;
	float cy = cam.y - hit.y;
	float cz = cam.z - hit.z;
	float dist = (float)__builtin_sqrt((double)(cy * cy + cx * cx + cz * cz));
	if (g_SplitScreenGame) {
		dist = g_FXVulcanHitDistance / 2.5f;
	}
	ang = *Obj_Vtbl(v)->GetViewAng(v, &tmp);
	byte normal = 0;
	if (g_VulcanHitUseNormal && !*targetName && g_VulcanRange * 0.8f > dist) {
		const vec3_t *p = v->vftable->GetPos(v, &tmp);
		Weap_Norm(&n, hit.x - p->x, hit.y - p->y, hit.z - p->z);
		float nx = n.x * 10.0f;
		float ny = n.y * 10.0f;
		float nz = n.z * 10.0f;
		to.x = nx + hit.x;
		from.x = hit.x - nx;
		to.y = ny + hit.y;
		from.y = hit.y - ny;
		to.z = nz + hit.z;
		from.z = hit.z - nz;
		if (Engine_TestLine(&from, &to, &out, &tri)) {
			hit = out;
			normal = 1;
			ang = *Vec3_GetRotAng(&tmp, &tri.plane.n);
		}
	}
	if (!player && !*targetName && g_VulcanHitUseNormal && !normal) {
		goto sound;
	}
	{
		float az = Vul_Rand(0.5, -0.25);
		float ay = Vul_Rand(0.5, -0.25);
		ang.x = Vul_Rand(0.5, -0.25) + ang.x;
		ang.y = ay + ang.y;
		ang.z = az + ang.z;
	}
	cDebrisSys *sys = cWorld_GetDebrisSys(g_pWorld, g_VulcanHitFile);
	cDebris *d = cDebrisSys_GetIdle(sys);
	wo = g_pWorld;
	cam = wo->camPos[wo->curCam];
	CGameModel_SetPos(Debris_Model(d), &cam);
	d->life = 0.6f;
	if (!cDebrisSys_IsIdle(sys, d)) {
		cDebrisSys_Drop(sys, d);
	}
	cObjElementFXParticleSystem *fx = Debris_Element(d);
	if (!fx) {
		goto target;
	}
	CGameParticles *p = &fx->particles;
	if (!fx->on) {
		fx->move = 0;
		fx->on = 1;
		CGameModel_SetSortZ(Debris_Model(d), -0x1.47ae14p-7f, 0.0f);
	}
	float k = 1.0f;
	float q = 1.0f - dist / g_FXVulcanHitDistance;
	float size = Float_MaxSS(0.85f, q * q * (dist / 2500.0f));
	if (*targetName) {
		cEntity *t = World_FindEntity(targetName);
		if (t) {
			cWorld *w = g_pWorld;
			if (w->owners[0]) {
				CHECK(Entity_IsType(w->owners[0], TYEN_WITH_LIFE));
			}
			if (t == w->owners[0] && Entity_IsType(t, TYEN_WITH_LIFE) && Obj(t)->cam) {
				cEntity *c = Obj(t)->cam;
				Cam_Shake(c) = 0x1.c98712p-7f;
				Cam_ShakeTime(c) = 0.06f;
				size = size + size;
				k = 1.5f;
			}
		}
	}
	Matrix_Identity(&mtx);
	Matrix_Rotate(&mtx, &ang);
	Matrix_Translate(&mtx, &hit);
	p->birthRate = 0;
	{
		float oz = Vul_Rand(15.0, 75.0);
		float oy = Vul_Rand(70.0, -35.0);
		off.x = size * Vul_Rand(70.0, -35.0);
		off.y = size * oy;
		off.z = size * oz;
	}
	CGameParticles_SetColorRampOn(p, 1);
	CGameParticles_SetColorRamp(p, g_VulcanRamps[0]);
	vec3_t up = { 0.0f, 0.0f, 35.0f };
	Vul_PosOnly(p, &up);
	Vul_Puff(p, size, 25.0, 100.0, 50.0, 650.0, 0.15f, 0x505, 0.019999995827674866, 0.20000000298023224);
	CGameParticles_Apply(p);
	CGameParticles_EmitM(p, 1, 1, &mtx);
	Vul_PosOnly(p, &off);
	Vul_Puff(p, size, 25.0, 110.0, 30.0, 260.0, 0.3f, 0x202, 0.03999999165534973, 0.2800000011920929);
	CGameParticles_Apply(p);
	CGameParticles_EmitM(p, 1, 1, &mtx);
	if (*targetName) {
		Vul_PosOnly(p, &off);
		Vul_Puff(p, size, 20.0, 65.0, 50.0, 550.0, 0.2f, 0x101, 0.03999999165534973, 0.2199999988079071);
		CGameParticles_Apply(p);
		CGameParticles_EmitM(p, 1, 1, &mtx);
		Vul_PosOnly(p, &off);
		Vul_Puff(p, size, 25.0, 110.0, 150.0, 900.0, 0.15f, 0x505, 0.040000006556510925, 0.1599999964237213);
		float rot = Vul_Rand(6.2831854820251465, 0.0);
		p->rotIni = rot;
		p->rotEnd = rot;
		p->rotVar = Vul_Rand(0.7853981852531433, -0.39269909262657166);
		CGameParticles_Apply(p);
		CGameParticles_EmitM(p, 1, 1, &mtx);
	}
	p->initPosMin = off;
	p->initPosMax = off;
	Vul_Sparks(p, &mtx, dist, size, k, *targetName != 0);
	if (!*targetName) {
		Vul_Dust(p, &mtx, &off, size);
	}
target:
	if (*targetName) {
		cEntity *t = World_FindEntity(targetName);
		if (t) {
			cSndItem *s = PtrArray_Get(&WLife(t)->hitSounds, crt_rand() % 2);
			if (s) {
				s->volume = s->vols[0];
				s->pitch = s->pitches[0];
				cSndItem_Play(s, v);
			}
			Vulcan_Push(v, t, 0);
			return;
		}
	}
sound:
	Vul_Ricochet(&hit);
}

/* ship name, player, item name: the HUD item spins while the upgrade 2 fires, else it turns back */
void Vulcan_HudNative(cAbstractFile *f) {
	char name[0x80], itemName[0x80];
	int player;

	cAbstractFile_ReadString(f, name, 0x80);
	f->vftable->Read(f, &player, 4);
	cAbstractFile_ReadString(f, itemName, 0x80);
	cEntity *v = Weap_Vehicle(name);
	cCannonScorer *item = (cCannonScorer *)cScorer_FindItem(Scorer_Get(player), itemName);
	if (!item) {
		return;
	}
	float delta = Scorer_Get(player)->frameTime;
	if (Veh(v)->controls.fire &&
		cWeaponManager_GetUpgrade(&Veh(v)->weapons, g_VulcanIDWeap) == VULCAN_UPG_WIDE) {
		item->angle = item->angle + Scorer_Get(player)->frameTime * 0x1.921fb6p+1f;
	} else {
		Weap_HudSpinDown(item, delta * 0x1.921fb6p+3f);
	}
	item->size[0] = 20.0f;
	item->size[1] = 20.0f;
	item->color = Weap_CannonColor();
}

/* ship name: the shot (not on clients) hurts what it aims at */
void Vulcan_FireNative(cAbstractFile *f) {
	char name[0x80];
	vec3_t ang, out;
	cEntity *target;
	float dist;

	cAbstractFile_ReadString(f, name, 0x80);
	cEntity *v = Weap_Vehicle(name);
	Vulcan_Shoot(v);
	Vulcan_AimOn(v);
	if (g_pApp->flags & APPF_CLIENT) {
		return;
	}
	cTargetLock *wm = &Veh(v)->weapons;
	int n = cWeaponManager_NumSlots(wm, g_VulcanIDWeap, 1);
	if (!n) {
		return;
	}
	target = NULL;
	ang.x = Veh(v)->autoAim.ax;
	ang.y = Veh(v)->autoAim.ay;
	ang.z = 0.0f;
	dist = g_VulcanRange;
	v->vftable->GetTargetAngle(v, &ang.x, dist, &target);
	v->vftable->AimWeapon(v, ang.x, ang.y, &dist, &out, &target, 8);
	byte isCar = target && Entity_IsType(target, TYEN_VEHICLE);
	byte isLife = target && Entity_IsType(target, TYEN_WITH_LIFE);
	if (isCar) {
		cVehicleAttack_ShotAt(Veh_HitInfo(v), target->name, 1);
	} else {
		cVehicleAttack_ShotAtPoint(Veh_HitInfo(v), &out, 1);
	}
	if (isLife) {
		int damage = v->vftable->GetAI(v) && World_IsEasySingle() ? g_VulcanDamageEasy : g_VulcanDamage;
		if (!WLife(target)->god) {
			WLife(target)->life -= damage * n;
		}
		if (!isCar) {
			Vulcan_Push(v, target, n);
		}
	}
	/* the upgrades 1 and 2 use up their own ammo, at 0 the script restores the weapon */
	int level = cWeaponManager_GetUpgrade(wm, g_VulcanIDWeap);
	if ((uint32_t)(level - VULCAN_UPG_FAST) <= 1) {
		float u = (float)((cWeaponManager_GetUAmmo(wm, g_VulcanIDWeap)) - ((float)n));
		if (0.0f >= u) {
			cWeaponManager_SetUpgrade(wm, g_VulcanIDWeap, 0);
			u = 0.0f;
			cPy_Function_AddParamI(g_RestoreUpgradeFunc, level);
			cPy_Function_Execute(g_RestoreUpgradeFunc, 0);
		}
		cWeaponManager_SetUAmmo(wm, g_VulcanIDWeap, u);
	}
	float rest = (float)((cTargetLock_GetAmmo(wm, g_VulcanIDAmmo)) - ((float)n * g_VulcanAmmoCost));
	cWeaponManager_SetAmmo(wm, g_VulcanIDAmmo, Float_MaxSS(0.0f, rest));
}

void Vulcan_Push(cEntity *v, cEntity *e, int n) {
	if (!e || !Entity_IsType(e, TYEN_WITH_LIFE) || Entity_IsType(e, TYEN_WALKCHAR)) {
		return;
	}
	Weap_PushAway(v, e, n, g_VulcanHitForce);
}

void Vulcan_AimOnNative(cAbstractFile *f) {
	char name[0x80];

	cAbstractFile_ReadString(f, name, 0x80);
	Vulcan_AimOn(Weap_Vehicle(name));
}

void Vulcan_AimOffNative(cAbstractFile *f) {
	Weap_SetAimMode(f, 0);
}

/* the aim help of the Vulcan: wider with the upgrade 2 and for a pad with VehicleCenterPadLock */
void Vulcan_AimOn(cEntity *v) {
	Veh(v)->autoAim.status = 1;
	if (g_VehicleCenterPadLock != 0.0f && cVehicleEntity_GetOwnerSlot(v) != -1) {
		if (cWeaponManager_GetUpgrade(&Veh(v)->weapons, g_VulcanIDWeap) == VULCAN_UPG_WIDE) {
			Veh(v)->autoAim.speed = 0x1.9b0f58p+0f;
			cAutoAim_SetAngle(Veh_Aim(v), 0x1.893012p-2f);
		} else {
			Veh(v)->autoAim.speed = 0x1.226c3cp+0f;
			cAutoAim_SetAngle(Veh_Aim(v), 0x1.657184p-2f);
		}
		Veh(v)->autoAim.distLimit = g_VulcanDistLimitUPG;
		return;
	}
	if (cWeaponManager_GetUpgrade(&Veh(v)->weapons, g_VulcanIDWeap) == VULCAN_UPG_WIDE) {
		Veh(v)->autoAim.speed = 0x1.80406ep+0f;
		cAutoAim_SetAngle(Veh_Aim(v), 0x1.41b2f8p-2f);
		Veh(v)->autoAim.distLimit = g_VulcanDistLimitUPG;
		return;
	}
	Veh(v)->autoAim.speed = 0x1.80406ep-1f;
	cAutoAim_SetAngle(Veh_Aim(v), 0x1.f46bbap-3f);
	Veh(v)->autoAim.distLimit = g_VulcanDistLimit;
}

cGVar GVar_VulcanIDAmmo;
cGVar GVar_FXVulcanHitDistance;
cGVar GVar_VulcanTimeDelay;
cGVar GVar_VulcanRic2Sound;
cGVar GVar_VulcanRicSoundAttIni;
cGVar GVar_VulcanTimeDelayUPG;
cGVar GVar_VulcanHitForce;
cGVar GVar_VulcanDamageEasy;
cGVar GVar_VulcanHitFile;
cGVar GVar_VulcanRic1Sound;
cGVar GVar_VulcanRic0Sound;
cGVar GVar_VulcanAmmoCost;
cGVar GVar_VulcanBulletFile;
cGVar GVar_VulcanDamage;
cGVar GVar_VulcanHitUseNormal;
cGVar GVar_VulcanRic0SoundVol;
cGVar GVar_VulcanRic2SoundVol;
cGVar GVar_VulcanRic1SoundVol;
cGVar GVar_VulcanRange;
cGVar GVar_VulcanRicSoundAttEnd;
cPy_NativeFunc NativeFunc_VulcanPress;
cPy_NativeFunc NativeFunc_VulcanRelease;
cPy_NativeFunc NativeFunc_VulcanCreate;
cPy_NativeFunc NativeFunc_VulcanFire;
cPy_NativeFunc NativeFunc_VulcanFX;

/* the dynamic initializers of the globals here, run in the order of src/standalone/crtxc.inc */
INIT_GVAR_INT(Init_GVar_VulcanIDAmmo, &GVar_VulcanIDAmmo, "VulcanIDAmmo", 0, "The default weapon idx for the vulcan", 0)
FINI_GVAR_STRING(Fini_GVar_VulcanBulletFile, &GVar_VulcanBulletFile)
INIT_GVAR_STRING(Init_GVar_VulcanBulletFile, Fini_GVar_VulcanBulletFile, &GVar_VulcanBulletFile, "VulcanBulletFile", 0, "default bullet model for the vulcan", "Models/Weapons/Debris/Bullet/Bullet.M3D")
FINI_GVAR_STRING(Fini_GVar_VulcanHitFile, &GVar_VulcanHitFile)
INIT_GVAR_STRING(Init_GVar_VulcanHitFile, Fini_GVar_VulcanHitFile, &GVar_VulcanHitFile, "VulcanHitFile", 0, "default hit model for the vulcan", "Models/Weapons/Vulcan/Hit.M3D")
INIT_GVAR_FLOAT(Init_GVar_VulcanHitForce, &GVar_VulcanHitForce, "VulcanHitForce", 0, "The force that have a single vulcan shoot", 2000000.0f)
INIT_GVAR_FLOAT(Init_GVar_VulcanRange, &GVar_VulcanRange, "VulcanRange", 0, "The maximun range that have the vulcan", 40000.0f)
INIT_GVAR_FLOAT(Init_GVar_VulcanAmmoCost, &GVar_VulcanAmmoCost, "VulcanAmmoCost", 0, "Cost Per hit at vulcan", 0.5f)
INIT_GVAR_FLOAT(Init_GVar_VulcanTimeDelay, &GVar_VulcanTimeDelay, "VulcanTimeDelay", 0, "Time between vulcan shoots", 0.1f)
INIT_GVAR_FLOAT(Init_GVar_VulcanTimeDelayUPG, &GVar_VulcanTimeDelayUPG, "VulcanTimeDelayUPG", 0, "Time between vulcan shoots (UPGRADE)", 0.075f)
INIT_GVAR_INT(Init_GVar_VulcanDamage, &GVar_VulcanDamage, "VulcanDamage", 0, "Vulcan damage per slot", 2)
INIT_GVAR_INT(Init_GVar_VulcanDamageEasy, &GVar_VulcanDamageEasy, "VulcanDamageEasy", 0, "Vulcan damage per slot (easy Set)", 1)
FINI_GVAR_STRING(Fini_GVar_VulcanRic0Sound, &GVar_VulcanRic0Sound)
INIT_GVAR_STRING(Init_GVar_VulcanRic0Sound, Fini_GVar_VulcanRic0Sound, &GVar_VulcanRic0Sound, "VulcanRic0Sound", 0, "First Vulcan puchuing sound name", "VulcanRic0")
INIT_GVAR_FLOAT(Init_GVar_VulcanRic0SoundVol, &GVar_VulcanRic0SoundVol, "VulcanRic0SoundVol", 0, "First Vulcan puchuing sound volume", 1.0f)
FINI_GVAR_STRING(Fini_GVar_VulcanRic1Sound, &GVar_VulcanRic1Sound)
INIT_GVAR_STRING(Init_GVar_VulcanRic1Sound, Fini_GVar_VulcanRic1Sound, &GVar_VulcanRic1Sound, "VulcanRic1Sound", 0, "Second Vulcan puchuing sound name", "VulcanRic1")
INIT_GVAR_FLOAT(Init_GVar_VulcanRic1SoundVol, &GVar_VulcanRic1SoundVol, "VulcanRic1SoundVol", 0, "Second Vulcan puchuing sound volume", 1.0f)
FINI_GVAR_STRING(Fini_GVar_VulcanRic2Sound, &GVar_VulcanRic2Sound)
INIT_GVAR_STRING(Init_GVar_VulcanRic2Sound, Fini_GVar_VulcanRic2Sound, &GVar_VulcanRic2Sound, "VulcanRic2Sound", 0, "Third Vulcan puchuing sound name", "VulcanRic2")
INIT_GVAR_FLOAT(Init_GVar_VulcanRic2SoundVol, &GVar_VulcanRic2SoundVol, "VulcanRic2SoundVol", 0, "Third Vulcan puchuing sound volume", 1.0f)
INIT_GVAR_FLOAT(Init_GVar_VulcanRicSoundAttIni, &GVar_VulcanRicSoundAttIni, "VulcanRicSoundAttIni", 0, "Vulcan puchuing sound att ini", 0.0f)
INIT_GVAR_FLOAT(Init_GVar_VulcanRicSoundAttEnd, &GVar_VulcanRicSoundAttEnd, "VulcanRicSoundAttEnd", 0, "Vulcan puchuing sound att end", 0.0f)
INIT_GVAR_FLOAT(Init_GVar_FXVulcanHitDistance, &GVar_FXVulcanHitDistance, "FXVulcanHitDistance", 0, "Distance of Vulcan Sparks Effect", 80000.0f)
INIT_GVAR_INT(Init_GVar_VulcanHitUseNormal, &GVar_VulcanHitUseNormal, "VulcanHitUseNormal", 0, "Use the Normal of Hitted point at Vulcan Hit", 1)
INIT_NATIVE_FUNC(Init_NativeFunc_VulcanPress, &NativeFunc_VulcanPress, "VulcanPress", Vulcan_AimOnNative)
INIT_NATIVE_FUNC(Init_NativeFunc_VulcanRelease, &NativeFunc_VulcanRelease, "VulcanRelease", Vulcan_AimOffNative)
INIT_NATIVE_FUNC(Init_NativeFunc_VulcanCreate, &NativeFunc_VulcanCreate, "VulcanCreate", Vulcan_CreateNative)
INIT_NATIVE_FUNC(Init_NativeFunc_VulcanFire, &NativeFunc_VulcanFire, "VulcanFire", Vulcan_FireNative)
INIT_NATIVE_FUNC(Init_NativeFunc_VulcanFX, &NativeFunc_VulcanFX, "VulcanFX", Vulcan_HudNative)
