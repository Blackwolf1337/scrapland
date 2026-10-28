/*
 * Entities/Vehicle/Weaps/Swarm.cpp of the game: the Swarm, a missile of bee missiles (SwarmNumber1..4 per mounted
 * bay) that follow the target, see include/scrap/weaps2.h. The shot handler of the Tesla follows in the same code.
 */
#include "scrap/ai.h"
#include "scrap/crt.h"
#include "scrap/weaps2.h"

/* missile name, position x y z: a bee died there */
void Swarm_OnBeeHitNative(cAbstractFile *f) {
	char name[0x80];
	float x, y, z;

	cAbstractFile_ReadString(f, name, 0x80);
	f->vftable->Read(f, &x, 4);
	f->vftable->Read(f, &y, 4);
	f->vftable->Read(f, &z, 4);
	cEntity *m = Weap_Missile(name);
	if (Ent_Life(m) > 0) {
		Swarm_BeeDeath(m, x, y, z, 1);
	}
	cPrimitive_SetSize(Mis_Size(m), Mis_Radius(m) * 0.5f);
}

/* the death effect of a bee (fewer particles far from the camera) and its sound */
void Swarm_BeeDeath(cEntity *sw, float x, float y, float z, byte sound) {
	vec3_t rd;

	CHECK(sw);
	cWorld *w = g_pWorld;
	vec3_t cam = w->camPos[w->curCam];
	float dy = cam.y - y;
	float dx = cam.x - x;
	float dz = cam.z - z;
	float dist = (float)__builtin_sqrt((double)(dy * dy + dx * dx + dz * dz));
	if (!CGameModel_IsVisible(Debris_Model(Mis_Debris(sw))) && dist > 5000.0f) {
		return;
	}
	int n = 3 - (int)(dist / 15000.0f);
	if (n < 1) {
		n = 1;
	} else if (n > 3) {
		n = 3;
	}
	float time = (float)((double)(crt_rand() & 0x7fff) * g_WeapRandScale * 0.15000003576278687 + 0.6499999761581421);
	float size = (float)((double)(crt_rand() & 0x7fff) * g_WeapRandScale * 0.13999998569488525 + 0.18000000715255737);
	float r = (float)((double)(crt_rand() & 0x7fff) * g_WeapRandScale * 20.0 + 10.0);
	const vec3_t *d = FX_RandomDir(&rd);
	FXSwarm_BeeDeath(d->x * r + x, d->y * r + y, d->z * r + z, 0.0f, 0.0f, 0.0f, size, time, n);
	if (sound) {
		cSoundDesc desc;
		vec3_t pos;
		pos.x = x;
		pos.y = y;
		pos.z = z;
		cSoundDesc_Init(&desc);
		desc.param1 = g_SwarmBeeSoundAttIni;
		desc.param2 = g_SwarmBeeSoundAttEnd;
		desc.param3 = 0.0f;
		cSoundDesc_SetPos(&desc, &pos);
		desc.volume += Snd_Volume(g_SwarmBeeSoundVol);
		g_pSound->vftable->PlaySound(g_pSound, g_SwarmBeeSound, &desc, NULL);
	}
}

void Swarm_ShootNative(cAbstractFile *f) {
	char name[0x80];

	cAbstractFile_ReadString(f, name, 0x80);
	Swarm_Shoot(name);
}

/* the reload sound, the delay of the next shot and the shoot effect at every mounted bay */
void Swarm_Shoot(const char *name) {
	cEntity *v = Weap_Vehicle(name);
	if (cTargetLock_GetAmmo(Veh_Weapons(v), g_SwarmIDAmmo) >= 1.0f) {
		Weap_PlaySound(v, 9);
		Veh_FireTime(v) = -g_SwarmTimeDelay;
		for (int i = 0; i < 4; i++) {
			if (cTargetLock_HasSlot(Veh_Weapons(v), g_SwarmIDWeap, i)) {
				cVehicleEffects_Shoot(Veh_Effects(v), g_SwarmIDWeap, i);
			}
		}
	}
}

/* name, position, orientation */
void Swarm_NetCreateNative(cAbstractFile *f) {
	char name[0x80];
	vec3_t pos, dir;

	cAbstractFile_ReadString(f, name, 0x80);
	Weap_ReadVec(f, &pos);
	Weap_ReadVec(f, &dir);
	Swarm_NetCreate(name, pos, dir);
}

cEntity *Swarm_NetCreate(const char *name, vec3_t pos, vec3_t dir) {
	cEntity *m = cWorld_CreateEntity(g_pWorld, name, pos.x, pos.y, pos.z, TYEN_MISSILE);
	CHECK(m);
	CHECK(Entity_IsType(m, TYEN_MISSILE));
	cMissileEntity_SetHiddenSwarm((cMissileEntity *)m, 0);
	cMissileEntity_SetHideMain((cMissileEntity *)m, 1);
	Weap_Vtbl(m)->SetOrient(m, &dir);
	cMissileEntity_SetSwarmSpeed((cMissileEntity *)m, g_SwarmBeesSpeed);
	cMissileEntity_SetLifeTime((cMissileEntity *)m, g_SwarmLifeTime);
	cPy_Function_SetNative(Mis_OnBeeHit(m), (nativefunc_t)GAME_FUNC(Swarm_OnBeeHitNative, 0x125b50));
	cPy_Function_SetNative(Mis_OnHit(m), (nativefunc_t)GAME_FUNC(Swarm_HitFXNative, 0x126aa0));
	cEntity_SetNetResource(m, "Swarm");
	return m;
}

void Swarm_HudNative(cAbstractFile *f) {
	Weap_UpdateHud(f, g_SwarmHudTarget, g_SwarmHudTime, 4, 3);
}

/* the ship shoots: a missile with one bee per ammo (up to the bays) */
void Swarm_FireNative(cAbstractFile *f) {
	char name[0x80], obj[0x80];
	vec3_t pos, ang, vel;

	cAbstractFile_ReadString(f, name, 0x80);
	Swarm_Shoot(name);
	cEntity *v = Weap_Vehicle(name);
	float ammo = cTargetLock_GetAmmo(Veh_Weapons(v), g_SwarmIDAmmo);
	if (!(ammo >= 1.0f)) {
		return;
	}
	Weap_Vtbl(v)->GetPos(v, &pos);
	Weap_Vtbl(v)->GetOrient(v, &ang);
	Weap_Vtbl(v)->GetVel(v, &vel);
	Net_GetObjName(obj);
	cEntity *m = Swarm_NetCreate(obj, pos, ang);
	if (!m) {
		return;
	}
	Controls_RumbleA(Veh_Kick(v), 1.0f, 0.3f);
	Controls_RumbleB(Veh_Kick(v), 0.5f, 0.5f);
	cPy_Function_SetNative(Mis_OnHit(m), (nativefunc_t)GAME_FUNC(Swarm_ServerHitNative, 0x127430));
	cPy_Function_SetNative(Ent_OnDeath(m), (nativefunc_t)GAME_FUNC(Swarm_OnDeathNative, 0x126fc0));
	cPy_Function_SetNative(Ent_OnDamage(m), (nativefunc_t)GAME_FUNC(Swarm_OnDamageNative, 0x126e80));
	if (!Ent_God(m)) {
		Ent_Life(m) = g_SwarmNumber1 * g_SwarmArmor;
	}
	cPrimitive_SetSize(Mis_Size(m), 50.0f);
	cMissileEntity_SetParent((cMissileEntity *)m, name);
	cMissileEntity_SetTarget((cMissileEntity *)m, Nto1Node_TargetName(Veh_TargetLink(v)));
	Swarm_NetSetUp(m->name);
	int n = (int)ammo;
	if (Mis_Bees(m) > n) {
		Weap_Vtbl(m)->SetBees(m, n);
	}
	cPrimitive_SetSize(Mis_Size(m), Mis_Radius(m) * 0.5f);
	if (!Ent_God(m)) {
		Ent_Life(m) = Mis_Bees(m) * g_SwarmArmor;
	}
	cMissileEntity_SetLifeTime((cMissileEntity *)m, g_SwarmLifeTime - 0.2f);
	cWeaponManager_SetAmmo(Veh_Weapons(v), g_SwarmIDAmmo, (float)n - (float)Mis_Bees(m));
}

/* missile name, ship, target */
void Swarm_HitFXNative(cAbstractFile *f) {
	char name[0x80], ship[0x80], target[0x80];

	cAbstractFile_ReadString(f, name, 0x80);
	cAbstractFile_ReadString(f, ship, 0x80);
	cAbstractFile_ReadString(f, target, 0x80);
	Swarm_HitFX(name, ship, target);
}

/* the hit effect at the missile (more particles with more bees) and its sound; returns the missile */
cEntity *Swarm_HitFX(const char *name, const char *ship, const char *target) {
	vec3_t c, vel, dir, rd;

	(void)ship;
	(void)target;
	cEntity *m = Weap_Missile(name);
	const vec3_t *p = Weap_Vtbl(m)->GetCenter(m, &c);
	c = *p;
	float k = (float)(Mis_Bees(m) / 11) * 0.55f;
	const vec3_t *v = Weap_Vtbl(m)->GetVel(m, &vel);
	float len = (float)__builtin_sqrt((double)(v->x * v->x + v->y * v->y + v->z * v->z));
	if (len == 0.0f) {
		dir = *v;
	} else {
		float inv = 1.0f / len;
		dir.x = v->x * inv;
		dir.y = v->y * inv;
		dir.z = v->z * inv;
	}
	float time = (float)((double)(crt_rand() & 0x7fff) * g_WeapRandScale * 0.10000002384185791 + 0.8999999761581421);
	float size = (float)((double)(crt_rand() & 0x7fff) * g_WeapRandScale * 0.10000002384185791 + 0.8999999761581421);
	float r = (float)((double)(crt_rand() & 0x7fff) * g_WeapRandScale * 60.0 + 32.0);
	const vec3_t *d = FX_RandomDir(&rd);
	FXSwarm_Hit(d->x * r + c.x, d->y * r + c.y, d->z * r + c.z, 0.0f, 0.0f, 0.0f, size * 0.75f + k, time, dir.x,
		dir.y, dir.z);
	cSoundDesc desc;
	cSoundDesc_Init(&desc);
	desc.param1 = g_SwarmHitSoundAttIni;
	desc.param2 = g_SwarmHitSoundAttEnd;
	desc.param3 = 0.0f;
	cSoundDesc_SetPos(&desc, &c);
	desc.volume += Snd_Volume(g_SwarmHitSoundVol);
	g_pSound->vftable->PlaySound(g_pSound, g_SwarmHitSound, &desc, NULL);
	return m;
}

/* missile name, attacker name, damage: one bee lost per SwarmArmor; the target goes when all would die */
void Swarm_OnDamageNative(cAbstractFile *f) {
	char name[0x80], other[0x80];
	int damage;

	cAbstractFile_ReadString(f, name, 0x80);
	cAbstractFile_ReadString(f, other, 0x80);
	f->vftable->Read(f, &damage, 4);
	cEntity *m = Weap_Missile(name);
	int bees;
	if (Ent_Active(m) && (bees = Mis_Bees(m)) > 0) {
		int armor = g_SwarmArmor;
		if (damage >= armor) {
			int n = damage / armor;
			if (n > bees) {
				cMissileEntity_SetTarget((cMissileEntity *)m, "");
				Weap_Vtbl(m)->KillBees(m, Mis_Bees(m), 0);
			} else {
				Weap_Vtbl(m)->KillBees(m, n, 0);
			}
		}
	}
}

void Swarm_OnDeathNative(cAbstractFile *f) {
	char name[0x80], other[0x80];

	cAbstractFile_ReadString(f, name, 0x80);
	cAbstractFile_ReadString(f, other, 0x80);
	cEntity *m = Weap_Missile(name);
	if (Ent_Active(m)) {
		Weap_Vtbl(m)->SetActive(m, 0);
		Swarm_Hit(name, "", "");
		Weap_Vtbl(m)->SetBees(m, 0);
	}
}

void Swarm_AimOffNative(cAbstractFile *f) {
	Weap_SetAimMode(f, 0);
}

void Swarm_AimOnNative(cAbstractFile *f) {
	cEntity *v = Weap_SetAimMode(f, 2);
	cAutoAim_SetRange(Veh_Aim(v), 40000.0f);
	cAutoAim_SetAngle(Veh_Aim(v), 0x1.aceeap-2f);
}

void Swarm_AimMode3Native(cAbstractFile *f) {
	Weap_SetAimMode(f, 3);
}

void Swarm_AimMode2Native(cAbstractFile *f) {
	Weap_SetAimMode(f, 2);
}

/* missile name, ship, target */
void Swarm_ServerHitNative(cAbstractFile *f) {
	char name[0x80], ship[0x80], target[0x80];

	name[0] = 0;
	ship[0] = 0;
	target[0] = 0;
	cAbstractFile_ReadString(f, name, 0x80);
	cAbstractFile_ReadString(f, ship, 0x80);
	cAbstractFile_ReadString(f, target, 0x80);
	Swarm_Hit(name, ship, target);
}

/*
 * the hit: the effect; the ship that shot it is hurt and pushed back when it is near; the target (an entity with
 * life) is hurt and pushed by the bees; the hit info of the ship (the one named, a vehicle) gets the target or the
 * point of the missile
 */
void Swarm_Hit(const char *name, const char *ship, const char *target) {
	vec3_t t1, t2, t3, nd, vel;

	cEntity *sw = Swarm_HitFX(name, ship, target);
	if (!sw) {
		return;
	}
	cEntity *owner = World_FindEntity(Mis_Owner(sw));
	if (owner && Entity_IsType(owner, TYEN_VEHICLE)) {
		const vec3_t *ps = Weap_Vtbl(sw)->GetPos(sw, &t1);
		const vec3_t *po = Weap_Vtbl(owner)->GetPos(owner, &t2);
		float dx = po->x - ps->x;
		float dy = po->y - ps->y;
		float dz = po->z - ps->z;
		if (1000.0f > (float)__builtin_sqrt((double)(dy * dy + dx * dx + dz * dz))) {
			if (!Ent_God(owner)) {
				Ent_Life(owner) -= Mis_Bees(sw);
			}
			float k = (float)(Mis_Bees(sw) + 1) * g_SwarmHitForce / Ent_Mass(owner) * 5000.0f;
			if (!Entity_IsType(owner, TYEN_WALKCHAR)) {
				const vec3_t *n = Vec3_NormalizeTo(&nd, Weap_Vtbl(sw)->GetVel(sw, &t3));
				float px = n->x * k;
				float py = n->y * k;
				float pz = n->z * k;
				const vec3_t *ov = Weap_Vtbl(owner)->GetVel(owner, &t3);
				vel.y = ov->y - py;
				vel.z = ov->z - pz;
				vel.x = ov->x - px;
				Weap_Vtbl(owner)->SetVel(owner, &vel);
				cVehicleEntity_SetNetFlag(owner, 1);
			}
			cVehicleAttack_ShotAt(Veh_HitInfo(owner), owner->name, 0);
		}
	}
	if (target && *target) {
		cEntity *e = World_FindEntity(target);
		if (!e || !Entity_IsType(e, TYEN_WITH_LIFE)) {
			Con_Warnf("WARNING AT SWARM : '%s' Looser not exists!\n", target);
		} else {
			int mult = 4;
			if (owner && Weap_Vtbl(owner)->GetAI(owner) && World_IsEasySingle()) {
				mult = 2;
			}
			if (!Ent_God(e)) {
				Ent_Life(e) -= (Mis_Bees(sw) + 1) * mult;
			}
			if (ship && !*ship) {
				cVehicleAttack_ShotAt(Veh_HitInfo(e), target, 0);
			}
			float k = (float)(Mis_Bees(sw) + 1) * g_SwarmHitForce / Ent_Mass(e);
			const vec3_t *sv = Weap_Vtbl(sw)->GetVel(sw, &t3);
			float px = sv->x * k;
			float py = sv->y * k;
			float pz = sv->z * k;
			const vec3_t *ov = Weap_Vtbl(e)->GetVel(e, &t2);
			vel.y = ov->y + py;
			vel.z = ov->z + pz;
			vel.x = ov->x + px;
			Weap_Vtbl(e)->SetVel(e, &vel);
			if (Entity_IsType(e, TYEN_VEHICLE)) {
				cVehicleEntity_SetNetFlag(e, 1);
			}
		}
	} else {
		Weap_Vtbl(sw)->GetPos(sw, &t3);
	}
	if (ship && *ship) {
		cEntity *s = World_FindEntity(ship);
		if (s && Entity_IsType(s, TYEN_VEHICLE)) {
			CHECK(Entity_IsType(s, TYEN_VEHICLE));
			if (target && *target) {
				cVehicleAttack_ShotAt(Veh_HitInfo(s), target, 0);
				return;
			}
			cVehicleAttack_ShotAtPoint(Veh_HitInfo(s), Weap_Vtbl(sw)->GetPos(sw, &t3), 0);
		}
	}
}

void Swarm_NetSetUpNative(cAbstractFile *f) {
	char name[0x80];

	cAbstractFile_ReadString(f, name, 0x80);
	Swarm_NetSetUp(name);
}

/* the model, speeds, bees (SwarmNumber1..4 per mounted bay of the ship), the upgrade and the engine sound */
void Swarm_NetSetUp(const char *name) {
	vec3_t tmp, ang, vel;
	matrix_t mtx;

	cEntity *m = Weap_Missile(name);
	Weap_Vtbl(m)->GetPos(m, &tmp);
	Weap_Vtbl(m)->SetModel(m, g_SwarmMissileFile, 1.0f, 1.0f, 1.0f, 0);
	cMissileEntity_SetRadius((cMissileEntity *)m, 200.0f);
	cMissileEntity_SetTopRadius((cMissileEntity *)m, 500.0f);
	cMissileEntity_SetRadiusGrow((cMissileEntity *)m, 2000.0f);
	cMissileEntity_SetSpeed((cMissileEntity *)m, g_SwarmNormSpeed);
	cMissileEntity_SetTurnSpeed((cMissileEntity *)m, (float)(((double)g_SwarmTurnSpeed * WEAP_PI) / 180.0));
	cMissileEntity_SetFlag584((cMissileEntity *)m, 0);
	cEntity *v = Weap_FindVehicle(Mis_Owner(m));
	if (v) {
		float top = g_SwarmUpTopSpeed;
		if (Veh_Speed(v) > g_SwarmNormSpeed - top) {
			cMissileEntity_SetSpeed((cMissileEntity *)m, top + Veh_Speed(v));
		}
		cTargetLock *wm = Veh_Weapons(v);
		for (int i = 0; i < 4; i++) {
			if (!cTargetLock_HasSlot(wm, g_SwarmIDWeap, i)) {
				continue;
			}
			Weap_DropAtNode(v, Veh_WeapNodes(v)[i + g_SwarmIDAmmo * 4], &mtx, &ang, &vel);
			Weap_Vtbl(m)->AddBees(m, *g_SwarmSlotData[i]);
		}
		int level = cWeaponManager_GetUpgrade(wm, g_SwarmIDWeap);
		if (level) {
			float bees = (float)Mis_Bees(m);
			float left = cWeaponManager_GetUAmmo(wm, g_SwarmIDWeap) - (bees + bees);
			cWeaponManager_SetUAmmo(wm, g_SwarmIDWeap, left);
			if (level == 3) {
				cMissileEntity_SetSpeed((cMissileEntity *)m, g_SwarmFastSpeed);
				cMissileEntity_SetTurnSpeed((cMissileEntity *)m, (float)(((double)(g_SwarmTurnSpeed * 1.2f) * WEAP_PI) / 180.0));
				Mis_Type(m) = 1;
			} else if (level == 4) {
				cMissileEntity_SetTurnSpeed((cMissileEntity *)m, (float)(((double)g_SwarmTurnSpeedUPG * WEAP_PI) / 180.0));
				Mis_Type(m) = 2;
			}
			if (0.0f >= left) {
				cPy_Function_AddParamI(g_RestoreUpgradeFunc, level);
				cPy_Function_Execute(g_RestoreUpgradeFunc, 0);
				cWeaponManager_SetUpgrade(wm, g_SwarmIDWeap, 0);
			}
		}
	}
	Weap_Vtbl(m)->SetBees(m, cMissileEntity_GetSwarmCount((cMissileEntity *)m));
	int n = Mis_Bees(m);
	if (n > 0) {
		cMissileEntity_SetSwDthTime((cMissileEntity *)m, g_SwarmDeathTime / (float)n);
	}
	Weap_Vtbl(m)->SetMaxSpeed(m, 3000.0f);
	float r = (float)Mis_Bees(m) * g_SwarmBeeRadius + g_SwarmMinRadius;
	Mis_B7bd(m) = 1;
	cMissileEntity_SetRadius((cMissileEntity *)m, r * 0.25f);
	cMissileEntity_SetTopRadius((cMissileEntity *)m, r);
	Weap_EngineSound(m, g_SwarmEngineSound, g_SwarmEngineSoundVol, g_SwarmEngineSoundAttIni,
		g_SwarmEngineSoundAttEnd, g_SwarmEngineSoundDoppler);
}

/* Tesla.cpp (in the Swarm code): the shot of the Tesla, 0 without ammo; the angle range narrows without a hit */
byte Tesla_Shoot(cEntity *v) {
	if (!(cTargetLock_GetAmmo(Veh_Weapons(v), g_TeslaIDAmmo) > 0.0f)) {
		return 0;
	}
	Controls_RumbleA(Veh_Kick(v), 1.0f, 0.2f);
	int level = cWeaponManager_GetUpgrade(Veh_Weapons(v), g_TeslaIDWeap);
	float down;
	if (g_VehicleCenterPadLock != 0.0f && cVehicleEntity_GetOwnerSlot(v) != -1) {
		float min = level == 6 ? g_TeslaMinAngRangeUpgPad : g_TeslaMinAngRangePad;
		Veh_TeslaMinAng(v) = (float)(((double)min * WEAP_PI) / 180.0);
		down = g_TeslaDownRangeTimePad;
	} else {
		float min = level == 6 ? g_TeslaMinAngRangeUpg : g_TeslaMinAngRange;
		Veh_TeslaMinAng(v) = (float)(((double)min * WEAP_PI) / 180.0);
		down = g_TeslaDownRangeTime;
	}
	if (!*cHitInfo_GetName(Veh_HitInfo(v))) {
		Veh_TeslaAngSpeed(v) = -((Veh_TeslaAng(v) - Veh_TeslaMinAng(v)) / down);
	}
	if (Veh_Seat(v) == -1) {
		Weap_PlaySound(v, 10);
	}
	cWeaponManager_SetWeapSnd(Veh_Weapons(v), (level == 5) + 11);
	cWeaponManager_DelayFire(Veh_Weapons(v),
		Weap_Vtbl(v)->GetAI(v) && World_IsEasySingle() ? 0.15f : 0.05f);
	return 1;
}

/* the static initializer (rva 0x04b8b0): the table of the costs from the variables, which are made before it */
void Swarm_InitAmmoCosts(void) {
	g_SwarmAmmoCosts[0] = g_SwarmAmmoCost1;
	g_SwarmAmmoCosts[1] = g_SwarmAmmoCost2;
	g_SwarmAmmoCosts[2] = g_SwarmAmmoCost3;
	g_SwarmAmmoCosts[3] = g_SwarmAmmoCost4;
	g_SwarmAmmoCosts[4] = 0;
	g_SwarmAmmoCosts[5] = 0;
}
