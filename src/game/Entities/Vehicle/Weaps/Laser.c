/*
 * Entities/Vehicle/Weaps/Laser.cpp of the game: the laser shots and the Hook (the laser of the ship locks on an
 * entity or a point and pulls), see include/scrap/weaps2.h. The handlers of the Sonic mine (CreateSonicFunc,
 * SetUpSonicFunc) sit at the end of the same code.
 */
#include "scrap/crt.h"
#include "scrap/weaps2.h"
#include "scrap/staticinit.h"

#define g_LaserAmmoCost (GVar_LaserAmmoCost.value.f)
#define g_LaserShootFile (GVar_LaserShootFile.value.s)
#define g_LaserHitForce (GVar_LaserHitForce.value.f)
#define g_LaserHitFile (GVar_LaserHitFile.value.s)
#define g_HookEMITime (GVar_HookEMITime.value.f)
#define g_LaserTimeDelay (GVar_LaserTimeDelay.value.f)
#define g_LaserRicSoundVol (GVar_LaserRicSoundVol.value.f)
#define g_LaserHookFile (GVar_LaserHookFile.value.s)
#define g_LaserDamage (GVar_LaserDamage.value.i)
#define g_HookAccel (GVar_HookAccel.value.f)
#define g_LaserMinSpeed (GVar_LaserMinSpeed.value.f)
#define g_LaserRicSound (GVar_LaserRicSound.value.s)
#define g_SonicRechargeTime (GVar_SonicRechargeTime.value.f)
#define g_SonicArmor (GVar_SonicArmor.value.i)
#define g_FXSonicXpldeFile (GVar_FXSonicXpldeFile.value.s)
#define g_SonicLifeTime (GVar_SonicLifeTime.value.f)
#define g_SonicIDAmmo (GVar_SonicIDAmmo.value.i)
#define g_SonicAmmoNeed (GVar_SonicAmmoNeed.value.i)
#define g_CreateSonicFunc (((cPy_Function *)&GVar_CreateSonicFunc.func))
#define g_SetUpSonicFunc (((cPy_Function *)&GVar_SetUpSonicFunc.func))
static cGVar GVar_SonicRechargeTime;
static cGVar GVar_SonicLifeTime;
static cGVar GVar_SonicIDAmmo;
static cGVar GVar_SonicArmor;
static cGVar GVar_SonicAmmoNeed;
static cGVarFunction GVar_SetUpSonicFunc;
static cGVar GVar_LaserTimeDelay;
static cGVar GVar_LaserShootFile;
static cGVar GVar_LaserRicSoundVol;
static cGVar GVar_LaserRicSound;
static cGVar GVar_LaserMinSpeed;
static cGVar GVar_LaserHookFile;
static cGVar GVar_LaserHitForce;
static cGVar GVar_LaserHitFile;
static cGVar GVar_LaserDamage;
static cGVar GVar_LaserAmmoCost;
static cGVar GVar_HookEMITime;
static cGVar GVar_HookAccel;
static cGVar GVar_FXSonicXpldeFile;
static cGVarFunction GVar_CreateSonicFunc;
#define SND_HOOK 0x18		/* the sounds of the ship (cSndInterface at +0x144) */
#define SND_HOOK_START 0x17
#define SND_LASER_LOW 0x16

/* the name of the locked entity: cTargetLock_GetEntity returns the name, not the entity */
static inline const char *Laser_LockName(cTargetLock *lock) {
	return (const char *)cTargetLock_GetEntity(lock);
}

/* not on clients: the Hook locks on the entity with life it aims at, else on the point (none out of range) */
static void Hook_Aim(cEntity *v) {
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
			cWeaponManager_LockPoint(&Veh(v)->weapons, NULL);
		}
		return;
	}
	if (target && Entity_IsType(target, TYEN_WITH_LIFE)) {
		cWeaponManager_LockEntity(&Veh(v)->weapons, target->name);
		return;
	}
	cWeaponManager_LockPoint(&Veh(v)->weapons, &out);
}

static void Hook_Unlock(cEntity *v) {
	if (!(g_pApp->flags & APPF_CLIENT)) {
		cWeaponManager_LockPoint(&Veh(v)->weapons, NULL);
	}
}

static void Hook_AimNative(cAbstractFile *f) {
	char name[0x80];

	cAbstractFile_ReadString(f, name, 0x80);
	cEntity *v = Weap_FindVehicle(name);
	if (v) {
		Hook_Aim(v);
	}
}

static void Hook_Silence(cEntity *v) {
	cSndItem *s = PtrArray_Get(&Obj(v)->snd, SND_HOOK);
	if (s && s->playing) {
		s->volume = 0.0f;
		s->pitch = 0.0f;
	}
}

static void Hook_OffNative(cAbstractFile *f) {
	char name[0x80];

	cAbstractFile_ReadString(f, name, 0x80);
	cEntity *v = Weap_FindVehicle(name);
	if (v) {
		if (!(g_pApp->flags & APPF_CLIENT)) {
			cWeaponManager_LockPoint(&Veh(v)->weapons, NULL);
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
static byte Vehicle_IsLocalPlayer(cEntity *v);
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
static void Hook_StartSound(cEntity *v);
static void Hook_StopSound(cEntity *v);
static void Hook_UpdateNative(cAbstractFile *f) {
	char name[0x80];
	vec3_t tmp, hp;

	cAbstractFile_ReadString(f, name, 0x80);
	cEntity *v = Weap_FindVehicle(name);
	if (!v) {
		return;
	}
	byte isClient = g_pApp->flags & APPF_CLIENT;
	cTargetLock *wm = &Veh(v)->weapons;
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
	cSndItem *s = PtrArray_Get(&Obj(v)->snd, SND_HOOK);
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
			Veh(target)->emi = World_IsEasySingle() &&
				!target->vftable->GetAI(target) ? 0.25f : g_HookEMITime;
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
		Controls_RumbleB(&Veh(v)->controls, 0.15f, 0.4f);
	}
	if (pt) {
		if (!pullV) {
			return;
		}
		float k = g_HookAccel * 0.05f;
		Hook_AddVel(v, dx * k, dy * k, dz * k);
	} else {
		float share = WLife(target)->mass / (WLife(v)->mass + WLife(target)->mass);
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
static void Hook_StartNative(cAbstractFile *f) {
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
	cTargetLock *wm = &Veh(v)->weapons;
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
static byte Laser_Shoot(cEntity *v) {
	byte had = cTargetLock_GetAmmo(&Veh(v)->weapons, g_LaserIDAmmo) > g_LaserAmmoCost;
	if (cTargetLock_GetAmmo(&Veh(v)->weapons, g_LaserIDAmmo) > 0.0f) {
		cWeaponManager_DelayFire(&Veh(v)->weapons, g_LaserTimeDelay);
		if (cTargetLock_GetAmmo(&Veh(v)->weapons, g_LaserIDAmmo) > g_LaserAmmoCost * 2.0f) {
			Weap_PlaySound(v, SND_LASER_LOW);
		}
	}
	return had;
}

/*
 * ship name, hit point x y z, target name: the shot effect from every mounted laser bay to the point, then (when it
 * is seen) the hit particles at the point (on the surface of the target), the hit sound and the push
 */
static void Laser_Push(cEntity *v, cEntity *e, int n);
static void Laser_CreateNative(cAbstractFile *f) {
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
		if (!cTargetLock_HasSlot(&Veh(v)->weapons, g_LaserIDWeap, i)) {
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
		d->life = time;
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
			float rad = WLife(w)->prim.radius;
			const vec3_t *c = Obj_Vtbl(v)->GetViewPos(v, &tmp);
			Weap_Norm(&n, c->x - hit.x, c->y - hit.y, c->z - hit.z);
			float rr = (float)((double)(crt_rand() & 0x7fff) * RAND_SCALE * 0.30000001192092896 +
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
		d->life = 1.0f;
		if (!cDebrisSys_IsIdle(sys, d)) {
			cDebrisSys_Drop(sys, d);
		}
		cObjElementFXParticleSystem *fx = Debris_Element(d);
		if (fx) {
			CGameParticles *p = &fx->particles;
			if (!fx->on) {
				fx->move = 0;
				fx->on = 1;
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
					if (t == w->owners[0] && Entity_IsType(t, TYEN_WITH_LIFE) && Obj(t)->cam) {
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
			p->sizeEnd = (float)((double)(crt_rand() & 0x7fff) * RAND_SCALE * 50.0 + 200.0) * big;
			p->colorIni = 0xffc400;
			p->colorEnd = 0xffff8000;
			p->birthRate = 0;
			p->lifeTime = (float)((double)(crt_rand() & 0x7fff) * RAND_SCALE * 0.050000011920928955 + 0.25);
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
			p->sizeEnd = (float)((double)(crt_rand() & 0x7fff) * RAND_SCALE * 50.0 + 300.0) * big;
			p->colorIni = 0x40c48000;
			p->colorEnd = 0xffc48000;
			p->birthRate = 0;
			p->lifeTime = (float)((double)(crt_rand() & 0x7fff) * RAND_SCALE * 0.04999998211860657 +
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
			p->sizeEnd = (float)((double)(crt_rand() & 0x7fff) * RAND_SCALE * 100.0 + 400.0) * big;
			p->colorIni = 0x80c44000;
			p->colorEnd = 0xffc44000;
			p->birthRate = 0;
			p->lifeTime = (float)((double)(crt_rand() & 0x7fff) * RAND_SCALE * 0.050000011920928955 +
				0.550000011920929);
			CGameParticles_Apply(p);
			CGameParticles_Emit(p, 1, 1, time, &mtx);
		}
	}
	cEntity *t = *targetName ? World_FindEntity(targetName) : NULL;
	if (t) {
		int k = crt_rand() % 2;
		cSndItem *snd = PtrArray_Get(&WLife(t)->hitSounds, k + (shots - 1) / 2 * 2);
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
	if (g_LaserAmmoCost >= cTargetLock_GetAmmo(&Veh(v)->weapons, g_LaserIDAmmo)) {
		Weap_PlaySound(v, SND_LASER_LOW);
	}
}

/* ship name, player, item name: the HUD item of the laser fades with the ammo, blinks when it is low */
static void Laser_HudNative(cAbstractFile *f) {
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
	float time = Scorer_Get(player)->time;
	float ammo = cTargetLock_GetAmmo(&Veh(v)->weapons, g_LaserIDAmmo);
	cAmmoInfo *info = TABLE_AT(g_pWorld->ammo, g_LaserIDAmmo);
	int level = (int)(ammo * 255.0f / info->max);
	if (cTargetLock_GetAmmo(&Veh(v)->weapons, g_LaserIDAmmo) > g_LaserAmmoCost * 2.0f) {
		float k = (float)(0xff - level);
		int g = (int)g_CannonScorerColorGreen - (int)(k * 0.8f);
		int b = (int)g_CannonScorerColorBlue - (int)(k * 0.55f);
		if (g < 0) {
			g = 0;
		}
		if (b < 0) {
			b = 0;
		}
		item->color = (((uint32_t)g | (g_CannonScorerColorRed | 0xffffbf00u) << 8) << 8) | (uint32_t)b;
	} else {
		int k = (int)(time * 1024.0f) % 512 - 255;
		uint32_t a = (uint32_t)(k < 0 ? -k : k);
		item->color = ((((g_CannonScorerColorRed | 0xffffbf00u) << 8) | a) << 8) | a;
	}
	item->size[0] = 20.0f;
	item->size[1] = 20.0f;
	item->angle = 0.0f;
}

static void Laser_AimOn(cEntity *v) {
	Veh(v)->autoAim.status = 1;
	Veh(v)->autoAim.speed = 0x1.80406ep-1f;
	cAutoAim_SetAngle(Veh_Aim(v), 0x1.f46bbap-3f);
	Veh(v)->autoAim.distLimit = g_LaserDistLimit;
}

/* ship name: the shot (not on clients) hurts what it aims at, one LaserDamage per mounted bay */
static void Laser_FireNative(cAbstractFile *f) {
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
	int n = cWeaponManager_NumSlots(&Veh(v)->weapons, g_LaserIDWeap, 1);
	if (!n) {
		return;
	}
	ang.x = Veh(v)->autoAim.ax;
	ang.y = Veh(v)->autoAim.ay;
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
		if (!WLife(target)->god) {
			WLife(target)->life -= g_LaserDamage * n;
		}
		if (!isCar) {
			Laser_Push(v, target, n);
		}
	}
	float cost = g_LaserAmmoCost;
	float left = cTargetLock_GetAmmo(&Veh(v)->weapons, g_LaserIDAmmo) - (float)n * cost;
	if (cost > left) {
		left = 0.0f;
	}
	cWeaponManager_SetAmmo(&Veh(v)->weapons, g_LaserIDAmmo, left);
	Controls_RumbleB(&Veh(v)->controls, 1.0f, 0.2f);
}

/* the shot pushes an entity with life that walks not, away from the ship */
static void Laser_Push(cEntity *v, cEntity *e, int n) {
	if (!e || !Entity_IsType(e, TYEN_WITH_LIFE) || Entity_IsType(e, TYEN_WALKCHAR)) {
		return;
	}
	Weap_PushAway(v, e, n, g_LaserHitForce);
}

static void Laser_AimOnNative(cAbstractFile *f) {
	char name[0x80];

	cAbstractFile_ReadString(f, name, 0x80);
	Laser_AimOn(Weap_Vehicle(name));
}

static void Laser_AimOffNative(cAbstractFile *f) {
	Weap_SetAimMode(f, 0);
}

static void Hook_StopSound(cEntity *v) {
	Hook_Silence(v);
}

/* the Hook sound and its start sound with a recoil, unless it plays already */
static void Hook_StartSound(cEntity *v) {
	cSndItem *s = PtrArray_Get(&Obj(v)->snd, SND_HOOK);
	if (!s || s->playing) {
		return;
	}
	s->volume = s->vols[0];
	s->pitch = s->pitches[0];
	cSndItem_Play(s, v);
	s = PtrArray_Get(&Obj(v)->snd, SND_HOOK_START);
	if (s) {
		s->volume = s->vols[0];
		s->pitch = s->pitches[0];
		cSndItem_Play(s, v);
		Controls_RumbleB(&Veh(v)->controls, 0.2f, 0.2f);
	}
}

static byte Vehicle_IsLocalPlayer(cEntity *v) {
	return Veh_Player(v) >= 0 && Veh_Player(v) < 4;
}

/* ship name: CreateSonicFunc makes the mine (not on clients), up to SonicAmmoNeed ammo go into it */
static void Sonic_FireNative(cAbstractFile *f) {
	char name[0x80], obj[0x80];
	vec3_t pos, ang;

	cAbstractFile_ReadString(f, name, 0x80);
	cEntity *v = Weap_FindVehicle(name);
	if (!v) {
		return;
	}
	int ammo = (int)cTargetLock_GetAmmo(&Veh(v)->weapons, g_SonicIDAmmo);
	if (ammo < 1) {
		return;
	}
	Veh(v)->weapons.cmStamp = g_SonicRechargeTime;
	cSndItem *s = PtrArray_Get(&Obj(v)->snd, 0x13);
	if (s) {
		cSndItem *src = PtrArray_Get(&Obj(v)->snd, 9);
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
	Controls_RumbleA(&Veh(v)->controls, 0.4f, 0.2f);
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
	if (!WLife(m)->god) {
		WLife(m)->life = g_SonicArmor;
	}
	cMissileEntity_SetLifeTime((cMissileEntity *)m, g_SonicLifeTime - 0.2f);
	cPy_Function_SetFunc(&Missile(m)->onHit, "Sonic.OnServerHit");
	cPy_Function_SetFunc(&WLife(m)->onDeath, "Sonic.OnDeath");
	cWeaponManager_SetAmmo(&Veh(v)->weapons, g_SonicIDAmmo, (float)(ammo - Missile(m)->swarmCount / 3));
}

static vec3_t g_SonicHitPos;

/* the explosion of a mine at x y z (read into g_SonicHitPos) */
static void Sonic_ExplodeNative(cAbstractFile *f) {
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
	d->life = 2.0f;
	cDebrisSys_Drop(sys, d);
	cObjElementFXSonicExplosion *fx = Debris_Element(d);
	if (fx) {
		if (!fx->on) {
			fx->on = 1;
		}
		cObjElementFXSonicExplosion_Explode(fx, p->x, p->y, p->z);
	}
}

static cGVar GVar_LaserAmmoCost;
cGVar GVar_LaserRange;
static cGVar GVar_LaserShootFile;
static cGVar GVar_LaserHitForce;
static cGVar GVar_LaserHitFile;
static cGVar GVar_HookEMITime;
static cGVar GVar_LaserTimeDelay;
static cGVar GVar_LaserRicSoundVol;
cGVar GVar_LaserIDAmmo;
static cGVar GVar_LaserHookFile;
static cGVar GVar_LaserDamage;
static cGVar GVar_HookAccel;
static cGVar GVar_LaserMinSpeed;
static cGVar GVar_LaserRicSound;
static cGVar GVar_SonicRechargeTime;
static cGVar GVar_SonicArmor;
static cGVar GVar_FXSonicXpldeFile;
static cGVar GVar_SonicLifeTime;
static cGVar GVar_SonicIDAmmo;
static cGVar GVar_SonicAmmoNeed;
static cGVarFunction GVar_CreateSonicFunc;
static cGVarFunction GVar_SetUpSonicFunc;
static cPy_NativeFunc NativeFunc_LaserPress;
static cPy_NativeFunc NativeFunc_LaserRelease;
static cPy_NativeFunc NativeFunc_LaserCreate;
static cPy_NativeFunc NativeFunc_LaserFire;
static cPy_NativeFunc NativeFunc_LaserFX;
static cPy_NativeFunc NativeFunc_HookPlugFX;
static cPy_NativeFunc NativeFunc_HookFire;
static cPy_NativeFunc NativeFunc_HookActivate;
static cPy_NativeFunc NativeFunc_HookDeActivate;
static cPy_NativeFunc NativeFunc_SonicXplde;
static cPy_NativeFunc NativeFunc_SonicFire;

/* the dynamic initializers of the globals here, run in the order of src/standalone/crtxc.inc */
INIT_GVAR_INT(Init_GVar_LaserIDAmmo, &GVar_LaserIDAmmo, "LaserIDAmmo", 0, "The default weapon idx for the laser", 0)
FINI_GVAR_STRING(Fini_GVar_LaserHitFile, &GVar_LaserHitFile)
INIT_GVAR_STRING(Init_GVar_LaserHitFile, Fini_GVar_LaserHitFile, &GVar_LaserHitFile, "LaserHitFile", 0, "default hit model for the laser", "Models/Weapons/Laser/Hit.M3D")
FINI_GVAR_STRING(Fini_GVar_LaserShootFile, &GVar_LaserShootFile)
INIT_GVAR_STRING(Init_GVar_LaserShootFile, Fini_GVar_LaserShootFile, &GVar_LaserShootFile, "LaserShootFile", 0, "default shoot model for the laser", "Models/Weapons/Laser/Shoot.M3D")
FINI_GVAR_STRING(Fini_GVar_LaserHookFile, &GVar_LaserHookFile)
INIT_GVAR_STRING(Init_GVar_LaserHookFile, Fini_GVar_LaserHookFile, &GVar_LaserHookFile, "LaserHookFile", 0, "default model for the hook", "Models/Weapons/Laser/Hook.M3D")
INIT_GVAR_FLOAT(Init_GVar_LaserHitForce, &GVar_LaserHitForce, "LaserHitForce", 0, "The force that have a single laser shoot", 2000000.0f)
INIT_GVAR_FLOAT(Init_GVar_LaserRange, &GVar_LaserRange, "LaserRange", 0, "The maximun range that have the laser", 30000.0f)
INIT_GVAR_FLOAT(Init_GVar_LaserAmmoCost, &GVar_LaserAmmoCost, "LaserAmmoCost", 0, "Cost Per hit at laser", 10.0f)
INIT_GVAR_FLOAT(Init_GVar_LaserTimeDelay, &GVar_LaserTimeDelay, "LaserTimeDelay", 0, "Time in seconds between the laser shoots", 0.2f)
INIT_GVAR_INT(Init_GVar_LaserDamage, &GVar_LaserDamage, "LaserDamage", 0, "Damage per shoot", 3)
INIT_GVAR_FLOAT(Init_GVar_LaserMinSpeed, &GVar_LaserMinSpeed, "LaserMinSpeed", 0, "The minimun Speed of the laser shoots", 50000.0f)
FINI_GVAR_STRING(Fini_GVar_LaserRicSound, &GVar_LaserRicSound)
INIT_GVAR_STRING(Init_GVar_LaserRicSound, Fini_GVar_LaserRicSound, &GVar_LaserRicSound, "LaserRicSound", 0, "First Laser puchuing sound name", "VulcanRic0")
INIT_GVAR_FLOAT(Init_GVar_LaserRicSoundVol, &GVar_LaserRicSoundVol, "LaserRicSoundVol", 0, "First Laser puchuing sound volume", 1.0f)
INIT_GVAR_FLOAT(Init_GVar_HookEMITime, &GVar_HookEMITime, "HookEMITime", 0, "Time of electromagetic interference of the Hook", 3.0f)
INIT_GVAR_FLOAT(Init_GVar_HookAccel, &GVar_HookAccel, "HookAccel", 0, "Acceleration when the hook is plugged at the world.", 20000.0f)
FINI_CALL(Fini_GVar_CreateSonicFunc, &GVar_CreateSonicFunc.func, cPy_Function_dtor)
INIT_GVAR_FUNCTION(Init_GVar_CreateSonicFunc, Fini_GVar_CreateSonicFunc, &GVar_CreateSonicFunc, "CreateSonicFunc", 0, "Function to be called to create the mine", "")
FINI_CALL(Fini_GVar_SetUpSonicFunc, &GVar_SetUpSonicFunc.func, cPy_Function_dtor)
INIT_GVAR_FUNCTION(Init_GVar_SetUpSonicFunc, Fini_GVar_SetUpSonicFunc, &GVar_SetUpSonicFunc, "SetUpSonicFunc", 0, "Function to be called to set up the mine properties", "")
INIT_GVAR_INT(Init_GVar_SonicIDAmmo, &GVar_SonicIDAmmo, "SonicIDAmmo", 0, "Sonic Ammo ID", 0)
INIT_GVAR_INT(Init_GVar_SonicAmmoNeed, &GVar_SonicAmmoNeed, "SonicAmmoNeed", 0, "Sonic Ammo Need", 0)
INIT_GVAR_INT(Init_GVar_SonicArmor, &GVar_SonicArmor, "SonicArmor", 0, "Sonic Armor", 0)
INIT_GVAR_FLOAT(Init_GVar_SonicRechargeTime, &GVar_SonicRechargeTime, "SonicRechargeTime", 0, "Sonic Recharge Time", 0.0f)
INIT_GVAR_FLOAT(Init_GVar_SonicLifeTime, &GVar_SonicLifeTime, "SonicLifeTime", 0, "Sonic Life Time", 0.0f)
FINI_GVAR_STRING(Fini_GVar_FXSonicXpldeFile, &GVar_FXSonicXpldeFile)
INIT_GVAR_STRING(Init_GVar_FXSonicXpldeFile, Fini_GVar_FXSonicXpldeFile, &GVar_FXSonicXpldeFile, "FXSonicXpldeFile", 0, "default Xplode model for the Sonic", "Models/Weapons/Sonic/Xplde.M3D")
INIT_NATIVE_FUNC(Init_NativeFunc_LaserPress, &NativeFunc_LaserPress, "LaserPress", Laser_AimOnNative)
INIT_NATIVE_FUNC(Init_NativeFunc_LaserRelease, &NativeFunc_LaserRelease, "LaserRelease", Laser_AimOffNative)
INIT_NATIVE_FUNC(Init_NativeFunc_LaserCreate, &NativeFunc_LaserCreate, "LaserCreate", Laser_CreateNative)
INIT_NATIVE_FUNC(Init_NativeFunc_LaserFire, &NativeFunc_LaserFire, "LaserFire", Laser_FireNative)
INIT_NATIVE_FUNC(Init_NativeFunc_LaserFX, &NativeFunc_LaserFX, "LaserFX", Laser_HudNative)
INIT_NATIVE_FUNC(Init_NativeFunc_HookPlugFX, &NativeFunc_HookPlugFX, "HookPlugFX", Hook_StartNative)
INIT_NATIVE_FUNC(Init_NativeFunc_HookFire, &NativeFunc_HookFire, "HookFire", Hook_UpdateNative)
INIT_NATIVE_FUNC(Init_NativeFunc_HookActivate, &NativeFunc_HookActivate, "HookActivate", Hook_AimNative)
INIT_NATIVE_FUNC(Init_NativeFunc_HookDeActivate, &NativeFunc_HookDeActivate, "HookDeActivate", Hook_OffNative)
INIT_NATIVE_FUNC(Init_NativeFunc_SonicXplde, &NativeFunc_SonicXplde, "SonicXplde", Sonic_ExplodeNative)
INIT_NATIVE_FUNC(Init_NativeFunc_SonicFire, &NativeFunc_SonicFire, "SonicFire", Sonic_FireNative)
