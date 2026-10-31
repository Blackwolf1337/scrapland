/*
 * Entities/Vehicle/Weaps/Laser.cpp of the game: the laser shots and the Hook (the laser of the ship locks on an
 * entity or a point and pulls), see include/scrap/weaps2.h. The handlers of the Sonic mine (CreateSonicFunc,
 * SetUpSonicFunc) sit at the end of the same code.
 */
#include "scrap/crt.h"
#include "scrap/weaps2.h"

#define SND_HOOK 0x18		/* the sounds of the ship (cSndInterface at +0x144) */
#define SND_HOOK_START 0x17
#define SND_LASER_LOW 0x16

/* the name of the locked entity: cTargetLock_GetEntity returns the name, not the entity */
static inline const char *Laser_LockName(cTargetLock *lock) {
	return (const char *)cTargetLock_GetEntity(lock);
}

/* not on clients: the Hook locks on the entity with life it aims at, else on the point (none out of range) */
void Hook_Aim(cEntity *v) {
	vec3_t ang, out;
	cEntity *target;
	float dist;

	if (g_pApp->flags & APPF_CLIENT) {
		return;
	}
	ang.x = 0.0f;
	ang.y = 0.0f;
	ang.z = 0.0f;
	target = NULL;
	dist = g_HookRange;
	v->vftable->GetTargetAngle(v, &ang.x, g_HookRange, &target);
	v->vftable->AimWeapon(v, ang.x, ang.y, &dist, &out, &target, 8);
	if (dist + 100.0f > g_HookRange) {
		if (!(g_pApp->flags & APPF_CLIENT)) {
			cWeaponManager_LockPoint(Veh_Weapons(v), NULL);
		}
		return;
	}
	if (target && Entity_IsType(target, TYEN_WITH_LIFE)) {
		cWeaponManager_LockEntity(Veh_Weapons(v), target->name);
		return;
	}
	cWeaponManager_LockPoint(Veh_Weapons(v), &out);
}

void Hook_Unlock(cEntity *v) {
	if (!(g_pApp->flags & APPF_CLIENT)) {
		cWeaponManager_LockPoint(Veh_Weapons(v), NULL);
	}
}

void Hook_AimNative(cAbstractFile *f) {
	char name[0x80];

	cAbstractFile_ReadString(f, name, 0x80);
	cEntity *v = Weap_FindVehicle(name);
	if (v) {
		Hook_Aim(v);
	}
}

static void Hook_Silence(cEntity *v) {
	cSndItem *s = PtrArray_Get(Ent_Sounds(v), SND_HOOK);
	if (s && s->playing) {
		s->volume = 0.0f;
		s->pitch = 0.0f;
	}
}

void Hook_OffNative(cAbstractFile *f) {
	char name[0x80];

	cAbstractFile_ReadString(f, name, 0x80);
	cEntity *v = Weap_FindVehicle(name);
	if (v) {
		if (!(g_pApp->flags & APPF_CLIENT)) {
			cWeaponManager_LockPoint(Veh_Weapons(v), NULL);
		}
		Hook_Silence(v);
	}
}

/* the speed of the entity, at least 16800 */
static float Hook_MaxSpeed(cEntity *e) {
	vec3_t tmp;
	const vec3_t *v = Obj_Vtbl(e)->GetVel(e, &tmp);
	return Float_MaxSS(16800.0f, (float)__builtin_sqrt((double)(v->x * v->x + v->y * v->y + v->z * v->z)));
}

static float Hook_Speed(cEntity *e) {
	vec3_t tmp;
	const vec3_t *v = Obj_Vtbl(e)->GetVel(e, &tmp);
	return (float)__builtin_sqrt((double)(v->x * v->x + v->y * v->y + v->z * v->z));
}

/* the velocity plus (x, y, z) */
static void Hook_AddVel(cEntity *e, float x, float y, float z) {
	vec3_t tmp, vel;
	const vec3_t *v = Obj_Vtbl(e)->GetVel(e, &tmp);
	vel.y = v->y + y;
	vel.z = v->z + z;
	vel.x = v->x + x;
	Obj_Vtbl(e)->SetVel(e, &vel);
}

static void Hook_ScaleVel(cEntity *e, float k) {
	vec3_t tmp, vel;
	const vec3_t *v = Obj_Vtbl(e)->GetVel(e, &tmp);
	vel.y = v->y * k;
	vel.z = v->z * k;
	vel.x = k * v->x;
	Obj_Vtbl(e)->SetVel(e, &vel);
}

/*
 * whether the pull is computed here for the entity: not for the ship of a network player that this machine does
 * not drive (a client computes no remote pulls)
 */
static byte Hook_Pulls(cEntity *e, byte isClient) {
	int idx = Net_GetPlayerIndex(e->name);
	byte car = Entity_IsType(e, TYEN_VEHICLE);
	if (idx == -1 || (car && Vehicle_IsLocalPlayer(e))) {
		return 1;
	}
	if (isClient || (idx < 0x100 && car)) {
		return 0;
	}
	return 1;
}

/*
 * ship name, every frame while the Hook fires: the ammo, the lock (on an entity with life or a point), the pull of
 * the ship towards it and of the entity towards the ship by their masses, each kept under its speed
 */
void Hook_UpdateNative(cAbstractFile *f) {
	char name[0x80];
	vec3_t tmp, hp;

	cAbstractFile_ReadString(f, name, 0x80);
	cEntity *v = Weap_FindVehicle(name);
	if (!v) {
		return;
	}
	byte isClient = g_pApp->flags & APPF_CLIENT;
	cTargetLock *wm = Veh_Weapons(v);
	int n = cWeaponManager_NumSlots(wm, g_LaserIDWeap, 1);
	if (!n) {
		return;
	}
	if (!isClient) {
		float fn = (float)n;
		float left = cTargetLock_GetAmmo(wm, g_LaserIDAmmo) - (fn + fn);
		if (g_LaserAmmoCost > left) {
			left = 1.0f;
		}
		cWeaponManager_SetAmmo(wm, g_LaserIDAmmo, left);
	}
	cTargetLock_SetHookDelay(wm, 0.05f);
	byte start = 1;
	cSndItem *s = PtrArray_Get(Ent_Sounds(v), SND_HOOK);
	if (s) {
		start = s->playing ^ 1;
	}
	if (g_LaserAmmoCost > cTargetLock_GetAmmo(wm, g_LaserIDAmmo)) {
		if (!(g_pApp->flags & APPF_CLIENT)) {
			cWeaponManager_LockPoint(wm, NULL);
		}
		Hook_StopSound(v);
		return;
	}
	Hook_StartSound(v);
	const char *lock = Laser_LockName(wm);
	const vec3_t *pt = cTargetLock_GetPoint(wm);
	if (!pt && !lock) {
		Hook_Aim(v);
		lock = Laser_LockName(wm);
		pt = cTargetLock_GetPoint(wm);
		if (!pt && !lock) {
			if (!start) {
				return;
			}
			for (int i = 0; i < 4; i++) {
				if (!cTargetLock_HasSlot(wm, g_LaserIDWeap, i)) {
					continue;
				}
				cDebrisSys *sys = cWorld_GetDebrisSys(g_pWorld, g_LaserHookFile);
				cDebris *d = cDebrisSys_NewElement(sys);
				cDebrisSys_Drop(sys, d);
				if (Debris_Element(d)) {
					cObjElementFXHook_StartPoint(Debris_Element(d), v, i);
				}
			}
			return;
		}
	}
	cEntity *target = NULL;
	const vec3_t *p = pt;
	if (!pt) {
		CHECK(lock);
		if (!*lock) {
			Hook_Unlock(v);
			return;
		}
		target = World_FindEntity(lock);
		if (!target) {
			Hook_Unlock(v);
			return;
		}
		if (Entity_IsType(target, TYEN_VEHICLE)) {
			Veh_EMITime(target) = World_IsEasySingle() && !target->vftable->GetAI(target) ? 0.25f : g_HookEMITime;
		}
		if (!Entity_IsType(target, TYEN_WITH_LIFE) || !Ent_Active(target)) {
			Hook_Unlock(v);
			return;
		}
		p = Obj_Vtbl(target)->GetViewPos(target, &tmp);
	}
	hp = *p;
	const vec3_t *c = Obj_Vtbl(v)->GetViewPos(v, &tmp);
	float dy = hp.y - c->y;
	float dx = hp.x - c->x;
	float dz = hp.z - c->z;
	float dist = (float)__builtin_sqrt((double)(dy * dy + dx * dx + dz * dz));
	if (dist + 100.0f > g_HookRange) {
		Hook_Unlock(v);
		return;
	}
	if (dist > 1.0f) {
		float inv = 1.0f / dist;
		dx *= inv;
		dy *= inv;
		dz *= inv;
	}
	float vmaxV = 0.0f;
	byte pullV = Hook_Pulls(v, isClient);
	if (pullV) {
		vmaxV = Hook_MaxSpeed(v);
		Controls_RumbleB(Vehicle_Controls(v), 0.15f, 0.4f);
	}
	if (pt) {
		if (!pullV) {
			return;
		}
		float k = g_HookAccel * 0.05f;
		Hook_AddVel(v, dx * k, dy * k, dz * k);
	} else {
		float share = Ent_Mass(target) / (Ent_Mass(v) + Ent_Mass(target));
		if (pullV) {
			float k = g_HookAccel * share * 0.05f;
			Hook_AddVel(v, dx * k, dy * k, dz * k);
		}
		if (Hook_Pulls(target, isClient)) {
			float vmaxT = Hook_MaxSpeed(target);
			float k = (share - 1.0f) * g_HookAccel * 0.05f;
			Hook_AddVel(target, dx * k, dy * k, dz * k);
			float sp = Hook_Speed(target);
			if (sp > vmaxT) {
				/* the original scales by speed / max here, not by max / speed */
				Hook_ScaleVel(target, sp / vmaxT);
			}
		}
	}
	if (!pullV) {
		return;
	}
	float sp = Hook_Speed(v);
	if (sp > vmaxV) {
		Hook_ScaleVel(v, vmaxV / sp);
	}
}

/* ship name, 1 for the start: the sound, then the Hook effect at every mounted laser bay */
void Hook_StartNative(cAbstractFile *f) {
	char name[0x80];
	int start;

	cAbstractFile_ReadString(f, name, 0x80);
	f->vftable->Read(f, &start, 4);
	cEntity *v = Weap_FindVehicle(name);
	if (!v) {
		return;
	}
	Weap_PlaySound(v, 0x1a - start);
	if (start != 1) {
		return;
	}
	cTargetLock *wm = Veh_Weapons(v);
	for (int i = 0; i < 4; i++) {
		if (!cTargetLock_HasSlot(wm, g_LaserIDWeap, i)) {
			continue;
		}
		cDebrisSys *sys = cWorld_GetDebrisSys(g_pWorld, g_LaserHookFile);
		cDebris *d = cDebrisSys_NewElement(sys);
		cDebrisSys_Drop(sys, d);
		cObjElementFXHook *fx = Debris_Element(d);
		if (fx) {
			if (!fx->on) {
				fx->on = 1;
			}
			const char *lock = Laser_LockName(wm);
			cEntity *target = NULL;
			if (lock && *lock) {
				HashTable *t = &g_pWorld->entities;
				HashNode *node = HashTable_Find(t, lock, HashTable_Hash(t, lock));
				if (node) {
					target = node->value;
				}
			}
			cObjElementFXHook_StartTarget(fx, v, target, i);
		}
	}
}

/* a laser shot: whether there was ammo for it; the delay and (with ammo for two more) the sound */
byte Laser_Shoot(cEntity *v) {
	byte had = cTargetLock_GetAmmo(Veh_Weapons(v), g_LaserIDAmmo) > g_LaserAmmoCost;
	if (cTargetLock_GetAmmo(Veh_Weapons(v), g_LaserIDAmmo) > 0.0f) {
		cWeaponManager_DelayFire(Veh_Weapons(v), g_LaserTimeDelay);
		if (cTargetLock_GetAmmo(Veh_Weapons(v), g_LaserIDAmmo) > g_LaserAmmoCost * 2.0f) {
			Weap_PlaySound(v, SND_LASER_LOW);
		}
	}
	return had;
}


/*
 * ship name, hit point x y z, target name: the shot effect from every mounted laser bay to the point, then (when it
 * is seen) the hit particles at the point (on the surface of the target), the hit sound and the push
 */
void Laser_CreateNative(cAbstractFile *f) {
	char name[0x80], targetName[0x80];
	vec3_t hit, s, rel, rot, n, tmp, cam, ang;
	matrix_t mtx;

	cAbstractFile_ReadString(f, name, 0x80);
	Weap_ReadVec(f, &hit);
	cAbstractFile_ReadString(f, targetName, 0x80);
	cEntity *v = Weap_Vehicle(name);
	float time = 0.2f;
	int shots = 0;
	float big = 1.0f;
	for (int i = 0; i < 4; i++) {
		if (!cTargetLock_HasSlot(Veh_Weapons(v), g_LaserIDWeap, i)) {
			continue;
		}
		const matrix_t *m = CGameModel_NodeMatrix(Obj_Vtbl(v)->GetModel(v), &mtx,
			Veh(v)->weapons.nodes[g_LaserIDAmmo][i], 0, NULL, 0);
		s.x = m->m[12];
		s.y = m->m[13];
		s.z = m->m[14];
		cDebrisSys *sys = cWorld_GetDebrisSys(g_pWorld, g_LaserShootFile);
		cDebris *d = cDebrisSys_NewElement(sys);
		d->drop[0] = s;
		rel.x = hit.x - s.x;
		rel.y = hit.y - s.y;
		rel.z = hit.z - s.z;
		const vec3_t *r = Vec3_GetRotAng(&rot, &rel);
		float inv = 1.0f / time;
		d->drop[1] = *r;
		float vx = (hit.x - s.x) * inv;
		float vy = (hit.y - s.y) * inv;
		float vz = (hit.z - s.z) * inv;
		d->drop[2].x = vx;
		d->drop[2].y = vy;
		d->drop[2].z = vz;
		float min = g_LaserMinSpeed;
		if (min > vx && min > vy && min > vz) {
			Weap_Norm(&n, hit.x - s.x, hit.y - s.y, hit.z - s.z);
			min = g_LaserMinSpeed;
			d->drop[2].x = n.x * min;
			d->drop[2].z = n.z * min;
			d->drop[2].y = n.y * min;
			float dist = Weap_Len(hit.x - s.x, hit.y - s.y, hit.z - s.z);
			time = dist / Weap_Len(d->drop[2].x, d->drop[2].y, d->drop[2].z);
		}
		d->drop[3].x = 0.0f;
		d->drop[3].y = 0.0f;
		d->drop[3].z = 0.0f;
		d->speed = time;
		cDebrisSys_Drop(sys, d);
		shots++;
		cObjElementFXLaser *fx = Debris_Element(d);
		if (fx) {
			if (!fx->on) {
				fx->on = 1;
			}
			cObjElementFXLaser_Shoot(fx, s.x, s.y, s.z, hit.x, hit.y, hit.z, time);
		}
		cVehicleEffects_Shoot(Veh_Effects(v), g_LaserIDWeap, i);
	}
	if (FX_IsVisible(hit.x, hit.y, hit.z, 500.0f, g_LaserDistLimit, NULL, 0)) {
		if (*targetName) {
			cEntity *w = World_FindEntity(targetName);
			CHECK(w);
			CHECK(Entity_IsType(w, TYEN_WITH_LIFE));
			float rad = Ent_Radius(w);
			const vec3_t *c = Obj_Vtbl(v)->GetViewPos(v, &tmp);
			Weap_Norm(&n, c->x - hit.x, c->y - hit.y, c->z - hit.z);
			float rr = (float)((double)(crt_rand() & 0x7fff) * g_WeapRandScale * 0.30000001192092896 +
				0.10000000149011612);
			hit.x = hit.x + n.x * rad * rr;
			hit.y = hit.y + n.y * rad * rr;
			hit.z = hit.z + n.z * rad * rr;
		}
		cDebrisSys *sys = cWorld_GetDebrisSys(g_pWorld, g_LaserHitFile);
		cDebris *d = cDebrisSys_GetIdle(sys);
		cWorld *wo = g_pWorld;
		cam = wo->camPos[wo->curCam];
		CGameModel_SetPos(Debris_Model(d), &cam);
		d->speed = 1.0f;
		if (!cDebrisSys_IsIdle(sys, d)) {
			cDebrisSys_Drop(sys, d);
		}
		byte *fx = Debris_Element(d);
		if (fx) {
			CGameParticles *p = (CGameParticles *)(fx + 4);
			if (!fx[0xb5]) {
				WFIELD(fx, uint16_t, 0xb4) = 0x100;
				CGameModel_SetSortZ(Debris_Model(d), -0x1.47ae14p-7f, 0.0f);
			}
			float scale = 1.0f;
			if (*targetName) {
				scale = 0.5f;
				cEntity *t = World_FindEntity(targetName);
				if (t) {
					cWorld *w = g_pWorld;
					if (w->owners[0]) {
						CHECK(Entity_IsType(w->owners[0], TYEN_WITH_LIFE));
					}
					if (t == w->owners[0] && Entity_IsType(t, TYEN_WITH_LIFE) && Ent_Cam(t)) {
						big = 2.0f;
					}
				}
			}
			Obj_Vtbl(v)->GetViewAng(v, &ang);
			Matrix_Identity(&mtx);
			Matrix_Rotate(&mtx, &ang);
			Matrix_Translate(&mtx, &hit);
			float zs = scale * 75.0f;
			p->initPosMin.x = 0.0f;
			p->initPosMin.y = 0.0f;
			p->initPosMin.z = zs;
			p->initPosMax.z = zs;
			float size = big * 10.0f;
			p->initPosMax.x = 0.0f;
			p->initPosMax.y = 0.0f;
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
			p->sizeIni = size;
			p->sizeLife = 0.2f;
			p->sizeEnd = (float)((double)(crt_rand() & 0x7fff) * g_WeapRandScale * 50.0 + 200.0) * big;
			p->colorIni = 0xffc400;
			p->colorEnd = 0xffff8000;
			p->birthRate = 0;
			p->lifeTime = (float)((double)(crt_rand() & 0x7fff) * g_WeapRandScale * 0.050000011920928955 + 0.25);
			CGameParticles_Apply(p);
			CGameParticles_Emit(p, 1, 1, time, &mtx);
			zs = scale * 90.0f;
			p->initPosMin.x = 0.0f;
			p->initPosMin.y = 0.0f;
			p->initPosMin.z = zs;
			p->initPosMax.x = 0.0f;
			p->initPosMax.z = zs;
			p->initPosMax.y = 0.0f;
			p->sizeIni = size;
			p->sizeLife = 0.25f;
			p->sizeEnd = (float)((double)(crt_rand() & 0x7fff) * g_WeapRandScale * 50.0 + 300.0) * big;
			p->colorIni = 0x40c48000;
			p->colorEnd = 0xffc48000;
			p->birthRate = 0;
			p->lifeTime = (float)((double)(crt_rand() & 0x7fff) * g_WeapRandScale * 0.04999998211860657 +
				0.4000000059604645);
			CGameParticles_Apply(p);
			CGameParticles_Emit(p, 1, 1, time, &mtx);
			zs = scale * 105.0f;
			p->initPosMin.x = 0.0f;
			p->initPosMin.y = 0.0f;
			p->initPosMin.z = zs;
			p->initPosMax.z = zs;
			p->initPosMax.x = 0.0f;
			p->initPosMax.y = 0.0f;
			p->sizeIni = size;
			p->sizeLife = 0.3f;
			p->sizeEnd = (float)((double)(crt_rand() & 0x7fff) * g_WeapRandScale * 100.0 + 400.0) * big;
			p->colorIni = 0x80c44000;
			p->colorEnd = 0xffc44000;
			p->birthRate = 0;
			p->lifeTime = (float)((double)(crt_rand() & 0x7fff) * g_WeapRandScale * 0.050000011920928955 +
				0.550000011920929);
			CGameParticles_Apply(p);
			CGameParticles_Emit(p, 1, 1, time, &mtx);
		}
	}
	cEntity *t = *targetName ? World_FindEntity(targetName) : NULL;
	if (t) {
		int k = crt_rand() % 2;
		cSndItem *snd = PtrArray_Get(Ent_HitSounds(t), k + (shots - 1) / 2 * 2);
		if (snd) {
			snd->volume = snd->vols[0];
			snd->pitch = snd->pitches[0];
			cSndItem_Play(snd, v);
		}
		Laser_Push(v, t, shots);
	} else {
		CSoundSys *ss = g_pSound;
		const char *ric = g_LaserRicSound;
		int vol = Snd_Volume(g_LaserRicSoundVol);
		ss->vftable->PlayAt(g_pSound, ric, &hit, vol);
	}
	if (g_LaserAmmoCost >= cTargetLock_GetAmmo(Veh_Weapons(v), g_LaserIDAmmo)) {
		Weap_PlaySound(v, SND_LASER_LOW);
	}
}

/* ship name, player, item name: the HUD item of the laser fades with the ammo, blinks when it is low */
void Laser_HudNative(cAbstractFile *f) {
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
	float time = Scorer_Get(player)->time;
	float ammo = cTargetLock_GetAmmo(Veh_Weapons(v), g_LaserIDAmmo);
	cAmmoInfo *info = TABLE_AT(g_pWorld->ammo, g_LaserIDAmmo);
	int level = (int)(ammo * 255.0f / info->max);
	if (cTargetLock_GetAmmo(Veh_Weapons(v), g_LaserIDAmmo) > g_LaserAmmoCost * 2.0f) {
		float k = (float)(0xff - level);
		int g = (int)g_CannonScorerColorGreen - (int)(k * 0.8f);
		int b = (int)g_CannonScorerColorBlue - (int)(k * 0.55f);
		if (g < 0) {
			g = 0;
		}
		if (b < 0) {
			b = 0;
		}
		Cannon_Color(item) = (((uint32_t)g | (g_CannonScorerColorRed | 0xffffbf00u) << 8) << 8) | (uint32_t)b;
	} else {
		int k = (int)(time * 1024.0f) % 512 - 255;
		uint32_t a = (uint32_t)(k < 0 ? -k : k);
		Cannon_Color(item) = ((((g_CannonScorerColorRed | 0xffffbf00u) << 8) | a) << 8) | a;
	}
	Cannon_SizeX(item) = 20.0f;
	Cannon_SizeY(item) = 20.0f;
	Cannon_Angle(item) = 0.0f;
}

static void Laser_AimOn(cEntity *v) {
	Veh_AimMode(v) = 1;
	Veh_AimCone(v) = 0x1.80406ep-1f;
	cAutoAim_SetAngle(Veh_Aim(v), 0x1.f46bbap-3f);
	Veh_AimDist(v) = g_LaserDistLimit;
}

/* ship name: the shot (not on clients) hurts what it aims at, one LaserDamage per mounted bay */
void Laser_FireNative(cAbstractFile *f) {
	char name[0x80];
	vec3_t ang, out;
	cEntity *target;
	float dist;

	cAbstractFile_ReadString(f, name, 0x80);
	cEntity *v = Weap_Vehicle(name);
	Laser_AimOn(v);
	if (!Laser_Shoot(v) || (g_pApp->flags & APPF_CLIENT)) {
		return;
	}
	int n = cWeaponManager_NumSlots(Veh_Weapons(v), g_LaserIDWeap, 1);
	if (!n) {
		return;
	}
	ang.x = Veh_AimAng(v);
	ang.y = Veh_AimAngY(v);
	ang.z = 0.0f;
	target = NULL;
	dist = g_LaserRange;
	v->vftable->GetTargetAngle(v, &ang.x, g_LaserRange, &target);
	v->vftable->AimWeapon(v, ang.x, ang.y, &dist, &out, &target, 8);
	byte isCar = target && Entity_IsType(target, TYEN_VEHICLE);
	byte isLife = target && Entity_IsType(target, TYEN_WITH_LIFE);
	if (isCar) {
		cVehicleAttack_ShotAt(Veh_HitInfo(v), target->name, 1);
	} else {
		cVehicleAttack_ShotAtPoint(Veh_HitInfo(v), &out, 1);
	}
	if (isLife) {
		if (!Ent_God(target)) {
			Ent_Life(target) -= g_LaserDamage * n;
		}
		if (!isCar) {
			Laser_Push(v, target, n);
		}
	}
	float cost = g_LaserAmmoCost;
	float left = cTargetLock_GetAmmo(Veh_Weapons(v), g_LaserIDAmmo) - (float)n * cost;
	if (cost > left) {
		left = 0.0f;
	}
	cWeaponManager_SetAmmo(Veh_Weapons(v), g_LaserIDAmmo, left);
	Controls_RumbleB(Vehicle_Controls(v), 1.0f, 0.2f);
}

/* the shot pushes an entity with life that walks not, away from the ship */
void Laser_Push(cEntity *v, cEntity *e, int n) {
	if (!e || !Entity_IsType(e, TYEN_WITH_LIFE) || Entity_IsType(e, TYEN_WALKCHAR)) {
		return;
	}
	Weap_PushAway(v, e, n, g_LaserHitForce);
}

void Laser_AimOnNative(cAbstractFile *f) {
	char name[0x80];

	cAbstractFile_ReadString(f, name, 0x80);
	Laser_AimOn(Weap_Vehicle(name));
}

void Laser_AimOffNative(cAbstractFile *f) {
	Weap_SetAimMode(f, 0);
}

void Hook_StopSound(cEntity *v) {
	Hook_Silence(v);
}

/* the Hook sound and its start sound with a recoil, unless it plays already */
void Hook_StartSound(cEntity *v) {
	cSndItem *s = PtrArray_Get(Ent_Sounds(v), SND_HOOK);
	if (!s || s->playing) {
		return;
	}
	s->volume = s->vols[0];
	s->pitch = s->pitches[0];
	cSndItem_Play(s, v);
	s = PtrArray_Get(Ent_Sounds(v), SND_HOOK_START);
	if (s) {
		s->volume = s->vols[0];
		s->pitch = s->pitches[0];
		cSndItem_Play(s, v);
		Controls_RumbleB(Vehicle_Controls(v), 0.2f, 0.2f);
	}
}

byte __fastcall Vehicle_IsLocalPlayer(cEntity *v) {
	return Veh_Player(v) >= 0 && Veh_Player(v) < 4;
}

/* ship name: CreateSonicFunc makes the mine (not on clients), up to SonicAmmoNeed ammo go into it */
void Sonic_FireNative(cAbstractFile *f) {
	char name[0x80], obj[0x80];
	vec3_t pos, ang;

	cAbstractFile_ReadString(f, name, 0x80);
	cEntity *v = Weap_FindVehicle(name);
	if (!v) {
		return;
	}
	int ammo = (int)cTargetLock_GetAmmo(Veh_Weapons(v), g_SonicIDAmmo);
	if (ammo < 1) {
		return;
	}
	Veh_FireTime2(v) = g_SonicRechargeTime;
	cSndItem *s = PtrArray_Get(Ent_Sounds(v), 0x13);
	if (s) {
		cSndItem *src = PtrArray_Get(Ent_Sounds(v), 9);
		if (src) {
			s->volume = src->vols[0];
			s->pitch = src->pitches[0];
			cSndItem_Play(s, v);
		}
	}
	if (g_pApp->flags & APPF_CLIENT) {
		return;
	}
	Net_GetObjName(obj);
	v->vftable->GetPos(v, &pos);
	Obj_Vtbl(v)->GetAng(v, &ang);
	cPy_Function_AddParamS(g_CreateSonicFunc, obj);
	cPy_Function_AddParamF(g_CreateSonicFunc, pos.x);
	cPy_Function_AddParamF(g_CreateSonicFunc, pos.y);
	cPy_Function_AddParamF(g_CreateSonicFunc, pos.z);
	cPy_Function_AddParamF(g_CreateSonicFunc, ang.x);
	cPy_Function_AddParamF(g_CreateSonicFunc, ang.y);
	cPy_Function_AddParamF(g_CreateSonicFunc, ang.z);
	cPy_Function_Execute(g_CreateSonicFunc, 0);
	cEntity *m = World_FindEntity(obj);
	if (!m) {
		return;
	}
	Controls_RumbleA(Vehicle_Controls(v), 0.4f, 0.2f);
	cMissileEntity_SetSpeed((cMissileEntity *)m, 0.0f);
	cMissileEntity_SetTurnSpeed((cMissileEntity *)m, 0.0f);
	g_DebrisDrop[0] = pos;
	g_DebrisDrop[3].x = 0x1.921fb6p+2f;
	g_DebrisDrop[3].y = 0x1.921fb6p+2f;
	g_DebrisDrop[3].z = 0x1.921fb6p+2f;
	int need = g_SonicAmmoNeed;
	cMissileEntity_SetCount((cMissileEntity *)m, (need > ammo ? ammo : need) * 3);
	cPy_Function_AddParamS(g_SetUpSonicFunc, m->name);
	cPy_Function_Execute(g_SetUpSonicFunc, 0);
	if (!Ent_God(m)) {
		Ent_Life(m) = g_SonicArmor;
	}
	cMissileEntity_SetLifeTime((cMissileEntity *)m, g_SonicLifeTime - 0.2f);
	cPy_Function_SetFunc(Mis_OnHit(m), "Sonic.OnServerHit");
	cPy_Function_SetFunc(Ent_OnDeath(m), "Sonic.OnDeath");
	cWeaponManager_SetAmmo(Veh_Weapons(v), g_SonicIDAmmo, (float)(ammo - Mis_Bees(m) / 3));
}

/* the explosion of a mine at x y z (read into g_SonicHitPos) */
void Sonic_ExplodeNative(cAbstractFile *f) {
	vec3_t *p = &g_SonicHitPos;

	f->vftable->Read(f, &p->x, 4);
	f->vftable->Read(f, &p->y, 4);
	f->vftable->Read(f, &p->z, 4);
	if (!FX_IsVisible(p->x, p->y, p->z, g_FXSonicRadius * 2.0f + 500.0f, 0.0f, NULL, 1)) {
		return;
	}
	cDebrisSys *sys = cWorld_GetDebrisSys(g_pWorld, g_FXSonicXpldeFile);
	cDebris *d = cDebrisSys_NewElement(sys);
	CGameModel_SetPos(Debris_Model(d), p);
	CGameModel_SetSortZ(Debris_Model(d), -50.0f, 0.0f);
	d->speed = 2.0f;
	cDebrisSys_Drop(sys, d);
	cObjElementFXSonicExplosion *fx = Debris_Element(d);
	if (fx) {
		if (!fx->on) {
			fx->on = 1;
		}
		cObjElementFXSonicExplosion_Explode(fx, p->x, p->y, p->z);
	}
}
