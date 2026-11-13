/*
 * Entities/Vehicle/Weaps/Swarm.cpp of the game: the Swarm, a missile of bee missiles (SwarmNumber1..4 per mounted
 * bay) that follow the target, see include/scrap/weaps2.h. The shot handler of the Tesla follows in the same code.
 */
#include "scrap/ai.h"
#include "scrap/crt.h"
#include "scrap/play.h"
#include "scrap/weaps2.h"
#include "scrap/staticinit.h"

#define g_SwarmHitSoundVol (GVar_SwarmHitSoundVol.value.f)
#define g_SwarmMissileFile (GVar_SwarmMissileFile.value.s)
#define g_SwarmUpTopSpeed (GVar_SwarmUpTopSpeed.value.f)
#define g_SwarmEngineSoundAttIni (GVar_SwarmEngineSoundAttIni.value.f)
#define g_SwarmBeeSoundAttEnd (GVar_SwarmBeeSoundAttEnd.value.f)
#define g_SwarmDeathTime (GVar_SwarmDeathTime.value.f)
#define g_SwarmBeeRadius (GVar_SwarmBeeRadius.value.f)
#define g_SwarmMinRadius (GVar_SwarmMinRadius.value.f)
#define g_SwarmBeesSpeed (GVar_SwarmBeesSpeed.value.f)
#define g_SwarmHitSound (GVar_SwarmHitSound.value.s)
#define g_SwarmHitSoundAttIni (GVar_SwarmHitSoundAttIni.value.f)
#define g_SwarmNormSpeed (GVar_SwarmNormSpeed.value.f)
#define g_SwarmLifeTime (GVar_SwarmLifeTime.value.f)
#define g_SwarmHitForce (GVar_SwarmHitForce.value.f)
#define g_SwarmTurnSpeedUPG (GVar_SwarmTurnSpeedUPG.value.f)
#define g_SwarmFastSpeed (GVar_SwarmFastSpeed.value.f)
#define g_SwarmTurnSpeed (GVar_SwarmTurnSpeed.value.f)
#define g_SwarmArmor (GVar_SwarmArmor.value.i)
#define g_SwarmEngineSound (GVar_SwarmEngineSound.value.s)
#define g_SwarmBeeSoundVol (GVar_SwarmBeeSoundVol.value.f)
#define g_SwarmEngineSoundVol (GVar_SwarmEngineSoundVol.value.f)
#define g_SwarmTimeDelay (GVar_SwarmTimeDelay.value.f)
#define g_SwarmBeeSoundAttIni (GVar_SwarmBeeSoundAttIni.value.f)
#define g_SwarmHitSoundAttEnd (GVar_SwarmHitSoundAttEnd.value.f)
#define g_SwarmBeeSound (GVar_SwarmBeeSound.value.s)
#define g_SwarmNumber1 (GVar_SwarmNumber1.value.i)
#define g_SwarmEngineSoundAttEnd (GVar_SwarmEngineSoundAttEnd.value.f)
#define g_SwarmEngineSoundDoppler (GVar_SwarmEngineSoundDoppler.value.f)
#define g_SwarmAmmoCost1 (GVar_SwarmAmmoCost1.value.i)
#define g_SwarmAmmoCost2 (GVar_SwarmAmmoCost2.value.i)
#define g_SwarmAmmoCost3 (GVar_SwarmAmmoCost3.value.i)
#define g_SwarmAmmoCost4 (GVar_SwarmAmmoCost4.value.i)
#define g_TeslaMinAngRangePad (GVar_TeslaMinAngRangePad.value.f)
#define g_TeslaMinAngRangeUpg (GVar_TeslaMinAngRangeUpg.value.f)
#define g_TeslaMinAngRangeUpgPad (GVar_TeslaMinAngRangeUpgPad.value.f)
static cGVar GVar_TeslaMinAngRangeUpgPad;
static cGVar GVar_TeslaMinAngRangeUpg;
static cGVar GVar_TeslaMinAngRangePad;
static cGVar GVar_SwarmUpTopSpeed;
static cGVar GVar_SwarmTurnSpeedUPG;
static cGVar GVar_SwarmTurnSpeed;
static cGVar GVar_SwarmTimeDelay;
static cGVar GVar_SwarmNumber1;
static cGVar GVar_SwarmNormSpeed;
static cGVar GVar_SwarmMissileFile;
static cGVar GVar_SwarmMinRadius;
static cGVar GVar_SwarmLifeTime;
static cGVar GVar_SwarmHitSoundVol;
static cGVar GVar_SwarmHitSoundAttIni;
static cGVar GVar_SwarmHitSoundAttEnd;
static cGVar GVar_SwarmHitSound;
static cGVar GVar_SwarmHitForce;
static cGVar GVar_SwarmFastSpeed;
static cGVar GVar_SwarmEngineSoundVol;
static cGVar GVar_SwarmEngineSoundDoppler;
static cGVar GVar_SwarmEngineSoundAttIni;
static cGVar GVar_SwarmEngineSoundAttEnd;
static cGVar GVar_SwarmEngineSound;
static cGVar GVar_SwarmDeathTime;
static cGVar GVar_SwarmBeesSpeed;
static cGVar GVar_SwarmBeeSoundVol;
static cGVar GVar_SwarmBeeSoundAttIni;
static cGVar GVar_SwarmBeeSoundAttEnd;
static cGVar GVar_SwarmBeeSound;
static cGVar GVar_SwarmBeeRadius;
static cGVar GVar_SwarmArmor;
static cGVar GVar_SwarmAmmoCost4;
static cGVar GVar_SwarmAmmoCost3;
static cGVar GVar_SwarmAmmoCost2;
static cGVar GVar_SwarmAmmoCost1;

/* missile name, position x y z: a bee died there */
static void Swarm_BeeDeath(cEntity *sw, float x, float y, float z, byte sound);
static void Swarm_OnBeeHitNative(cAbstractFile *f) {
	char name[0x80];
	float x, y, z;

	cAbstractFile_ReadString(f, name, 0x80);
	VFile_Read(f, &x, 4);
	VFile_Read(f, &y, 4);
	VFile_Read(f, &z, 4);
	cEntity *m = Weap_Missile(name);
	if (WLife(m)->life > 0) {
		Swarm_BeeDeath(m, x, y, z, 1);
	}
	cPrimitive_SetSize(Mis_Size(m), Missile(m)->topRadius * 0.5f);
}

/* the death effect of a bee (fewer particles far from the camera) and its sound */
static void Swarm_BeeDeath(cEntity *sw, float x, float y, float z, byte sound) {
	vec3_t rd;

	CHECK(sw);
	cWorld *w = g_pWorld;
	vec3_t cam = w->camPos[w->curCam];
	float dy = cam.y - y;
	float dx = cam.x - x;
	float dz = cam.z - z;
	float dist = (float)__builtin_sqrt((double)(dy * dy + dx * dx + dz * dz));
	if (!CGameModel_IsVisible(Debris_Model(Missile(sw)->swarm)) && dist > 5000.0f) {
		return;
	}
	int n = 3 - (int)(dist / 15000.0f);
	if (n < 1) {
		n = 1;
	} else if (n > 3) {
		n = 3;
	}
	float time = (float)((double)(crt_rand() & 0x7fff) * RAND_SCALE * 0.15000003576278687 + 0.6499999761581421);
	float size = (float)((double)(crt_rand() & 0x7fff) * RAND_SCALE * 0.13999998569488525 + 0.18000000715255737);
	float r = (float)((double)(crt_rand() & 0x7fff) * RAND_SCALE * 20.0 + 10.0);
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
		SoundSys_PlaySound(g_pSound, g_SwarmBeeSound, &desc, NULL);
	}
}

static void Swarm_Shoot(const char *name);
static void Swarm_ShootNative(cAbstractFile *f) {
	char name[0x80];

	cAbstractFile_ReadString(f, name, 0x80);
	Swarm_Shoot(name);
}

/* the reload sound, the delay of the next shot and the shoot effect at every mounted bay */
static void Swarm_Shoot(const char *name) {
	cEntity *v = Weap_Vehicle(name);
	if (cTargetLock_GetAmmo(&Veh(v)->weapons, g_SwarmIDAmmo) >= 1.0f) {
		Weap_PlaySound(v, 9);
		Veh(v)->weapons.fireStamp = -g_SwarmTimeDelay;
		for (int i = 0; i < 4; i++) {
			if (cTargetLock_HasSlot(&Veh(v)->weapons, g_SwarmIDWeap, i)) {
				cVehicleEffects_Shoot(Veh_Effects(v), g_SwarmIDWeap, i);
			}
		}
	}
}

/* name, position, orientation */
static void Swarm_NetCreateNative(cAbstractFile *f) {
	char name[0x80];
	vec3_t pos, dir;

	cAbstractFile_ReadString(f, name, 0x80);
	Weap_ReadVec(f, &pos);
	Weap_ReadVec(f, &dir);
	Swarm_NetCreate(name, pos, dir);
}

static void Swarm_HitFXNative(cAbstractFile *f);
cEntity *Swarm_NetCreate(const char *name, vec3_t pos, vec3_t dir) {
	cEntity *m = cWorld_CreateEntity(g_pWorld, name, pos.x, pos.y, pos.z, TYEN_MISSILE);
	CHECK(m);
	CHECK(Entity_IsType(m, TYEN_MISSILE));
	cMissileEntity_SetHiddenSwarm((cMissileEntity *)m, 0);
	cMissileEntity_SetHideMain((cMissileEntity *)m, 1);
	Obj_SetAng(m, &dir);
	cMissileEntity_SetSwarmSpeed((cMissileEntity *)m, g_SwarmBeesSpeed);
	cMissileEntity_SetLifeTime((cMissileEntity *)m, g_SwarmLifeTime);
	cPy_Function_SetNative(&Missile(m)->onBeeDeath, (nativefunc_t)Swarm_OnBeeHitNative);
	cPy_Function_SetNative(&Missile(m)->onHit, (nativefunc_t)Swarm_HitFXNative);
	cEntity_SetNetResource(m, "Swarm");
	return m;
}

static char g_SwarmHudTarget[4][0x80];
static float g_SwarmHudTime[4];

static void Swarm_HudNative(cAbstractFile *f) {
	Weap_UpdateHud(f, g_SwarmHudTarget, g_SwarmHudTime, 4, 3);
}

/* the ship shoots: a missile with one bee per ammo (up to the bays) */
static void Swarm_OnDamageNative(cAbstractFile *f);
static void Swarm_OnDeathNative(cAbstractFile *f);
static void Swarm_ServerHitNative(cAbstractFile *f);
static void Swarm_FireNative(cAbstractFile *f) {
	char name[0x80], obj[0x80];
	vec3_t pos, ang, vel;

	cAbstractFile_ReadString(f, name, 0x80);
	Swarm_Shoot(name);
	cEntity *v = Weap_Vehicle(name);
	float ammo = cTargetLock_GetAmmo(&Veh(v)->weapons, g_SwarmIDAmmo);
	if (!(ammo >= 1.0f)) {
		return;
	}
	Entity_GetPos(v, &pos);
	Obj_GetAng(v, &ang);
	Obj_GetVel(v, &vel);
	Net_GetObjName(obj);
	cEntity *m = Swarm_NetCreate(obj, pos, ang);
	if (!m) {
		return;
	}
	Controls_RumbleA(&Veh(v)->controls, 1.0f, 0.3f);
	Controls_RumbleB(&Veh(v)->controls, 0.5f, 0.5f);
	cPy_Function_SetNative(&Missile(m)->onHit, (nativefunc_t)Swarm_ServerHitNative);
	cPy_Function_SetNative(&WLife(m)->onDeath, (nativefunc_t)Swarm_OnDeathNative);
	cPy_Function_SetNative(&WLife(m)->onDamage, (nativefunc_t)Swarm_OnDamageNative);
	if (!WLife(m)->god) {
		WLife(m)->life = g_SwarmNumber1 * g_SwarmArmor;
	}
	cPrimitive_SetSize(Mis_Size(m), 50.0f);
	cMissileEntity_SetParent((cMissileEntity *)m, name);
	cMissileEntity_SetTarget((cMissileEntity *)m, Nto1Node_TargetName(&Veh(v)->autoAim.target));
	Swarm_NetSetUp(m->name);
	int n = (int)ammo;
	if (Missile(m)->swarmCount > n) {
		cMissileEntity_SetSwarm(Missile(m), n);
	}
	cPrimitive_SetSize(Mis_Size(m), Missile(m)->topRadius * 0.5f);
	if (!WLife(m)->god) {
		WLife(m)->life = Missile(m)->swarmCount * g_SwarmArmor;
	}
	cMissileEntity_SetLifeTime((cMissileEntity *)m, g_SwarmLifeTime - 0.2f);
	cWeaponManager_SetAmmo(&Veh(v)->weapons, g_SwarmIDAmmo, (float)n - (float)Missile(m)->swarmCount);
}

/* missile name, ship, target */
static cEntity *Swarm_HitFX(const char *name, const char *ship, const char *target);
static void Swarm_HitFXNative(cAbstractFile *f) {
	char name[0x80], ship[0x80], target[0x80];

	cAbstractFile_ReadString(f, name, 0x80);
	cAbstractFile_ReadString(f, ship, 0x80);
	cAbstractFile_ReadString(f, target, 0x80);
	Swarm_HitFX(name, ship, target);
}

/* the hit effect at the missile (more particles with more bees) and its sound; returns the missile */
static cEntity *Swarm_HitFX(const char *name, const char *ship, const char *target) {
	vec3_t c, vel, dir, rd;

	(void)ship;
	(void)target;
	cEntity *m = Weap_Missile(name);
	const vec3_t *p = Obj_GetViewPos(m, &c);
	c = *p;
	float k = (float)(Missile(m)->swarmCount / 11) * 0.55f;
	const vec3_t *v = Obj_GetVel(m, &vel);
	float len = (float)__builtin_sqrt((double)(v->x * v->x + v->y * v->y + v->z * v->z));
	if (len == 0.0f) {
		dir = *v;
	} else {
		float inv = 1.0f / len;
		dir.x = v->x * inv;
		dir.y = v->y * inv;
		dir.z = v->z * inv;
	}
	float time = (float)((double)(crt_rand() & 0x7fff) * RAND_SCALE * 0.10000002384185791 + 0.8999999761581421);
	float size = (float)((double)(crt_rand() & 0x7fff) * RAND_SCALE * 0.10000002384185791 + 0.8999999761581421);
	float r = (float)((double)(crt_rand() & 0x7fff) * RAND_SCALE * 60.0 + 32.0);
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
	SoundSys_PlaySound(g_pSound, g_SwarmHitSound, &desc, NULL);
	return m;
}

/* missile name, attacker name, damage: one bee lost per SwarmArmor; the target goes when all would die */
static void Swarm_OnDamageNative(cAbstractFile *f) {
	char name[0x80], other[0x80];
	int damage;

	cAbstractFile_ReadString(f, name, 0x80);
	cAbstractFile_ReadString(f, other, 0x80);
	VFile_Read(f, &damage, 4);
	cEntity *m = Weap_Missile(name);
	int bees;
	if (Ent_Active(m) && (bees = Missile(m)->swarmCount) > 0) {
		int armor = g_SwarmArmor;
		if (damage >= armor) {
			int n = damage / armor;
			if (n > bees) {
				cMissileEntity_SetTarget((cMissileEntity *)m, "");
				cMissileEntity_Hit(Missile(m), Missile(m)->swarmCount, 0);
			} else {
				cMissileEntity_Hit(Missile(m), n, 0);
			}
		}
	}
}

static void Swarm_Hit(const char *name, const char *ship, const char *target);
static void Swarm_OnDeathNative(cAbstractFile *f) {
	char name[0x80], other[0x80];

	cAbstractFile_ReadString(f, name, 0x80);
	cAbstractFile_ReadString(f, other, 0x80);
	cEntity *m = Weap_Missile(name);
	if (Ent_Active(m)) {
		Entity_SetActive(m, 0);
		Swarm_Hit(name, "", "");
		cMissileEntity_SetSwarm(Missile(m), 0);
	}
}

static void Swarm_AimOffNative(cAbstractFile *f) {
	Weap_SetAimMode(f, 0);
}

static void Swarm_AimOnNative(cAbstractFile *f) {
	cEntity *v = Weap_SetAimMode(f, 2);
	cAutoAim_SetRange(Veh_Aim(v), 40000.0f);
	cAutoAim_SetAngle(Veh_Aim(v), 0x1.aceeap-2f);
}

static void Swarm_AimMode3Native(cAbstractFile *f) {
	Weap_SetAimMode(f, 3);
}

static void Swarm_AimMode2Native(cAbstractFile *f) {
	Weap_SetAimMode(f, 2);
}

/* missile name, ship, target */
static void Swarm_ServerHitNative(cAbstractFile *f) {
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
static void Swarm_Hit(const char *name, const char *ship, const char *target) {
	vec3_t t1, t2, t3, nd, vel;

	cEntity *sw = Swarm_HitFX(name, ship, target);
	if (!sw) {
		return;
	}
	cEntity *owner = World_FindEntity(Missile(sw)->parent);
	if (owner && Entity_IsType(owner, TYEN_VEHICLE)) {
		const vec3_t *ps = Entity_GetPos(sw, &t1);
		const vec3_t *po = Entity_GetPos(owner, &t2);
		float dx = po->x - ps->x;
		float dy = po->y - ps->y;
		float dz = po->z - ps->z;
		if (1000.0f > (float)__builtin_sqrt((double)(dy * dy + dx * dx + dz * dz))) {
			if (!WLife(owner)->god) {
				WLife(owner)->life -= Missile(sw)->swarmCount;
			}
			float k = (float)(Missile(sw)->swarmCount + 1) * g_SwarmHitForce / WLife(owner)->mass * 5000.0f;
			if (!Entity_IsType(owner, TYEN_WALKCHAR)) {
				const vec3_t *n = Vec3_NormalizeTo(&nd, Obj_GetVel(sw, &t3));
				float px = n->x * k;
				float py = n->y * k;
				float pz = n->z * k;
				const vec3_t *ov = Obj_GetVel(owner, &t3);
				vel.y = ov->y - py;
				vel.z = ov->z - pz;
				vel.x = ov->x - px;
				Obj_SetVel(owner, &vel);
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
			if (owner && Entity_GetAI(owner) && World_IsEasySingle()) {
				mult = 2;
			}
			if (!WLife(e)->god && Rep_Hurt(owner, e, "Swarm")) {
				WLife(e)->life -= (Missile(sw)->swarmCount + 1) * mult;
			}
			if (ship && !*ship) {
				cVehicleAttack_ShotAt(Veh_HitInfo(e), target, 0);
			}
			float k = (float)(Missile(sw)->swarmCount + 1) * g_SwarmHitForce / WLife(e)->mass;
			const vec3_t *sv = Obj_GetVel(sw, &t3);
			float px = sv->x * k;
			float py = sv->y * k;
			float pz = sv->z * k;
			const vec3_t *ov = Obj_GetVel(e, &t2);
			vel.y = ov->y + py;
			vel.z = ov->z + pz;
			vel.x = ov->x + px;
			Obj_SetVel(e, &vel);
			if (Entity_IsType(e, TYEN_VEHICLE)) {
				cVehicleEntity_SetNetFlag(e, 1);
			}
		}
	} else {
		Entity_GetPos(sw, &t3);
	}
	if (ship && *ship) {
		cEntity *s = World_FindEntity(ship);
		if (s && Entity_IsType(s, TYEN_VEHICLE)) {
			CHECK(Entity_IsType(s, TYEN_VEHICLE));
			if (target && *target) {
				cVehicleAttack_ShotAt(Veh_HitInfo(s), target, 0);
				return;
			}
			cVehicleAttack_ShotAtPoint(Veh_HitInfo(s), Entity_GetPos(sw, &t3), 0);
		}
	}
}

static void Swarm_NetSetUpNative(cAbstractFile *f) {
	char name[0x80];

	cAbstractFile_ReadString(f, name, 0x80);
	Swarm_NetSetUp(name);
}

extern cGVar GVar_SwarmNumber2;
extern cGVar GVar_SwarmNumber3;
extern cGVar GVar_SwarmNumber4;
static cGVar GVar_SwarmNumber1;
static int *g_SwarmSlotData[4] = {
	&(&GVar_SwarmNumber1)->value.i, &(&GVar_SwarmNumber2)->value.i,
	&(&GVar_SwarmNumber3)->value.i, &(&GVar_SwarmNumber4)->value.i,
};

/* the model, speeds, bees (SwarmNumber1..4 per mounted bay of the ship), the upgrade and the engine sound */
void Swarm_NetSetUp(const char *name) {
	vec3_t tmp, ang, vel;
	matrix_t mtx;

	cEntity *m = Weap_Missile(name);
	Entity_GetPos(m, &tmp);
	Entity_SetModel(m, g_SwarmMissileFile, 1.0f, 1.0f, 1.0f, 0);
	cMissileEntity_SetRadius((cMissileEntity *)m, 200.0f);
	cMissileEntity_SetTopRadius((cMissileEntity *)m, 500.0f);
	cMissileEntity_SetRadiusGrow((cMissileEntity *)m, 2000.0f);
	cMissileEntity_SetSpeed((cMissileEntity *)m, g_SwarmNormSpeed);
	cMissileEntity_SetTurnSpeed((cMissileEntity *)m, (float)(((double)g_SwarmTurnSpeed * WEAP_PI) / 180.0));
	cMissileEntity_SetBeesGather((cMissileEntity *)m, 0);
	cEntity *v = Weap_FindVehicle(Missile(m)->parent);
	if (v) {
		float top = g_SwarmUpTopSpeed;
		if (Veh(v)->speed > g_SwarmNormSpeed - top) {
			cMissileEntity_SetSpeed((cMissileEntity *)m, top + Veh(v)->speed);
		}
		cTargetLock *wm = &Veh(v)->weapons;
		for (int i = 0; i < 4; i++) {
			if (!cTargetLock_HasSlot(wm, g_SwarmIDWeap, i)) {
				continue;
			}
			Weap_DropAtNode(v, Veh(v)->weapons.nodes[g_SwarmIDAmmo][i], &mtx, &ang, &vel);
			cMissileEntity_AddToSwarm(Missile(m), *g_SwarmSlotData[i]);
		}
		int level = cWeaponManager_GetUpgrade(wm, g_SwarmIDWeap);
		if (level) {
			float bees = (float)Missile(m)->swarmCount;
			float left = cWeaponManager_GetUAmmo(wm, g_SwarmIDWeap) - (bees + bees);
			cWeaponManager_SetUAmmo(wm, g_SwarmIDWeap, left);
			if (level == 3) {
				cMissileEntity_SetSpeed((cMissileEntity *)m, g_SwarmFastSpeed);
				cMissileEntity_SetTurnSpeed((cMissileEntity *)m, (float)(((double)(g_SwarmTurnSpeed * 1.2f) * WEAP_PI) / 180.0));
				Missile(m)->level = 1;
			} else if (level == 4) {
				cMissileEntity_SetTurnSpeed((cMissileEntity *)m, (float)(((double)g_SwarmTurnSpeedUPG * WEAP_PI) / 180.0));
				Missile(m)->level = 2;
			}
			if (0.0f >= left) {
				cPy_Function_AddParamI(g_RestoreUpgradeFunc, level);
				cPy_Function_Execute(g_RestoreUpgradeFunc, 0);
				cWeaponManager_SetUpgrade(wm, g_SwarmIDWeap, 0);
			}
		}
	}
	cMissileEntity_SetSwarm(Missile(m), cMissileEntity_GetSwarmCount((cMissileEntity *)m));
	int n = Missile(m)->swarmCount;
	if (n > 0) {
		cMissileEntity_SetSwDthTime((cMissileEntity *)m, g_SwarmDeathTime / (float)n);
	}
	WithLife_SetMass(m, 3000.0f);
	float r = (float)Missile(m)->swarmCount * g_SwarmBeeRadius + g_SwarmMinRadius;
	Missile(m)->fallOut = 1;
	cMissileEntity_SetRadius((cMissileEntity *)m, r * 0.25f);
	cMissileEntity_SetTopRadius((cMissileEntity *)m, r);
	Weap_EngineSound(m, g_SwarmEngineSound, g_SwarmEngineSoundVol, g_SwarmEngineSoundAttIni,
		g_SwarmEngineSoundAttEnd, g_SwarmEngineSoundDoppler);
}

/* Tesla.cpp (in the Swarm code): the shot of the Tesla, 0 without ammo; the angle range narrows without a hit */
byte Tesla_Shoot(cEntity *v) {
	if (!(cTargetLock_GetAmmo(&Veh(v)->weapons, g_TeslaIDAmmo) > 0.0f)) {
		return 0;
	}
	Controls_RumbleA(&Veh(v)->controls, 1.0f, 0.2f);
	int level = cWeaponManager_GetUpgrade(&Veh(v)->weapons, g_TeslaIDWeap);
	float down;
	if (g_VehicleCenterPadLock != 0.0f && cVehicleEntity_GetOwnerSlot(v) != -1) {
		float min = level == 6 ? g_TeslaMinAngRangeUpgPad : g_TeslaMinAngRangePad;
		Veh(v)->autoAim.loAng = (float)(((double)min * WEAP_PI) / 180.0);
		down = g_TeslaDownRangeTimePad;
	} else {
		float min = level == 6 ? g_TeslaMinAngRangeUpg : g_TeslaMinAngRange;
		Veh(v)->autoAim.loAng = (float)(((double)min * WEAP_PI) / 180.0);
		down = g_TeslaDownRangeTime;
	}
	if (!*cHitInfo_GetName(Veh_HitInfo(v))) {
		Veh(v)->autoAim.deltaAng = -((Veh(v)->autoAim.hiAng - Veh(v)->autoAim.loAng) / down);
	}
	if (Veh(v)->weapons.weapSnd == -1) {
		Weap_PlaySound(v, 10);
	}
	cWeaponManager_SetWeapSnd(&Veh(v)->weapons, (level == 5) + 11);
	cWeaponManager_DelayFire(&Veh(v)->weapons,
		Entity_GetAI(v) && World_IsEasySingle() ? 0.15f : 0.05f);
	return 1;
}

static int g_SwarmAmmoCosts[6];

/* the static initializer: the table of the costs from the variables, which are made before it */
void Swarm_InitAmmoCosts(void) {
	g_SwarmAmmoCosts[0] = g_SwarmAmmoCost1;
	g_SwarmAmmoCosts[1] = g_SwarmAmmoCost2;
	g_SwarmAmmoCosts[2] = g_SwarmAmmoCost3;
	g_SwarmAmmoCosts[3] = g_SwarmAmmoCost4;
	g_SwarmAmmoCosts[4] = 0;
	g_SwarmAmmoCosts[5] = 0;
}

cGVar GVar_SwarmIDWeap;
static cGVar GVar_SwarmHitSoundVol;
static cGVar GVar_SwarmMissileFile;
static cGVar GVar_SwarmUpTopSpeed;
static cGVar GVar_SwarmEngineSoundAttIni;
static cGVar GVar_SwarmBeeSoundAttEnd;
static cGVar GVar_SwarmDeathTime;
static cGVar GVar_SwarmBeeRadius;
static cGVar GVar_SwarmMinRadius;
static cGVar GVar_SwarmAmmoCost2;
static cGVar GVar_SwarmBeesSpeed;
static cGVar GVar_SwarmHitSound;
static cGVar GVar_SwarmHitSoundAttIni;
static cGVar GVar_SwarmNormSpeed;
static cGVar GVar_SwarmAmmoCost1;
static cGVar GVar_SwarmLifeTime;
static cGVar GVar_SwarmHitForce;
static cGVar GVar_SwarmTurnSpeedUPG;
static cGVar GVar_SwarmFastSpeed;
static cGVar GVar_SwarmTurnSpeed;
static cGVar GVar_SwarmAmmoCost3;
static cGVar GVar_SwarmArmor;
static cGVar GVar_SwarmAmmoCost4;
static cGVar GVar_SwarmEngineSound;
static cGVar GVar_SwarmBeeSoundVol;
cGVar GVar_SwarmIDAmmo;
static cGVar GVar_SwarmEngineSoundVol;
static cGVar GVar_SwarmTimeDelay;
static cGVar GVar_SwarmBeeSoundAttIni;
static cGVar GVar_SwarmHitSoundAttEnd;
static cGVar GVar_SwarmBeeSound;
static cGVar GVar_SwarmNumber1;
static cGVar GVar_SwarmEngineSoundAttEnd;
static cGVar GVar_SwarmEngineSoundDoppler;
cGVar GVar_TeslaDownRangeTimePad;
static cGVar GVar_TeslaMinAngRangePad;
cGVar GVar_TeslaDownRangeTime;
static cGVar GVar_TeslaMinAngRangeUpg;
static cGVar GVar_TeslaMinAngRangeUpgPad;
cGVar GVar_TeslaMinAngRange;
static cPy_NativeFunc NativeFunc_SwarmPress;
static cPy_NativeFunc NativeFunc_SwarmRelease;
static cPy_NativeFunc NativeFunc_SwarmOnEnd;
static cPy_NativeFunc NativeFunc_SwarmOnStart;
static cPy_NativeFunc NativeFunc_SwarmHit;
static cPy_NativeFunc NativeFunc_SwarmServerHit;
static cPy_NativeFunc NativeFunc_SwarmCreate;
static cPy_NativeFunc NativeFunc_SwarmSetUp;
static cPy_NativeFunc NativeFunc_SwarmClientFire;
static cPy_NativeFunc NativeFunc_SwarmFire;
static cPy_NativeFunc NativeFunc_SwarmFX;

/* the dynamic initializers of the globals here, run in the order of src/standalone/crtxc.inc */
INIT_GVAR_INT(Init_GVar_SwarmIDWeap, &GVar_SwarmIDWeap, "SwarmIDWeap", 0, "The default weapon idx for the Swarm", 6)
INIT_GVAR_INT(Init_GVar_SwarmIDAmmo, &GVar_SwarmIDAmmo, "SwarmIDAmmo", 0, "The default ammo idx for the swarm", 1)
INIT_GVAR_FLOAT(Init_GVar_SwarmLifeTime, &GVar_SwarmLifeTime, "SwarmLifeTime", 0, "The maximun lifetime that have the swarm", 4.5f)
INIT_GVAR_FLOAT(Init_GVar_SwarmDeathTime, &GVar_SwarmDeathTime, "SwarmDeathTime", 0, "The time for bee death that have the swarm", 0.5f)
INIT_GVAR_INT(Init_GVar_SwarmArmor, &GVar_SwarmArmor, "SwarmArmor", 0, "The default armor for one swarm missile", 2)
FINI_GVAR_STRING(Fini_GVar_SwarmMissileFile, &GVar_SwarmMissileFile)
INIT_GVAR_STRING(Init_GVar_SwarmMissileFile, Fini_GVar_SwarmMissileFile, &GVar_SwarmMissileFile, "SwarmMissileFile", 0, "Default Model for Swarm Missile", "Models/Weapons/Swarm/Swarm.M3D")
INIT_GVAR_FLOAT(Init_GVar_SwarmNormSpeed, &GVar_SwarmNormSpeed, "SwarmNormSpeed", 0, "The Formal Speed that have the swarm", 20000.0f)
INIT_GVAR_FLOAT(Init_GVar_SwarmFastSpeed, &GVar_SwarmFastSpeed, "SwarmFastSpeed", 0, "The Fast Speed that have the swarm", 40000.0f)
INIT_GVAR_FLOAT(Init_GVar_SwarmBeesSpeed, &GVar_SwarmBeesSpeed, "SwarmBeesSpeed", 0, "The Internal Bees Speed that have the swarm", 20000.0f)
INIT_GVAR_FLOAT(Init_GVar_SwarmUpTopSpeed, &GVar_SwarmUpTopSpeed, "SwarmUpTopSpeed", 0, "The Up Speed (Vehicle) that have the swarm", 4000.0f)
INIT_GVAR_FLOAT(Init_GVar_SwarmTurnSpeed, &GVar_SwarmTurnSpeed, "SwarmTurnSpeed", 0, "The Turn Speed that have the swarm", 60.0f)
INIT_GVAR_FLOAT(Init_GVar_SwarmTurnSpeedUPG, &GVar_SwarmTurnSpeedUPG, "SwarmTurnSpeedUPG", 0, "The Turn Speed that have the swarm with Upgrade", 100.0f)
INIT_GVAR_FLOAT(Init_GVar_SwarmHitForce, &GVar_SwarmHitForce, "SwarmHitForce", 0, "The Hit Force Energy that have the swarm", 200.0f)
INIT_GVAR_FLOAT(Init_GVar_SwarmTimeDelay, &GVar_SwarmTimeDelay, "SwarmTimeDelay", 0, "Time in seconds between the mis. shoots", 1.5f)
INIT_GVAR_FLOAT(Init_GVar_SwarmMinRadius, &GVar_SwarmMinRadius, "SwarmMinRadius", 0, "The Minimun Radius that have the swarm", 250.0f)
INIT_GVAR_FLOAT(Init_GVar_SwarmBeeRadius, &GVar_SwarmBeeRadius, "SwarmBeeRadius", 0, "The Radius add per Bee that have the swarm", 24.0f)
INIT_GVAR_INT(Init_GVar_SwarmAmmoCost1, &GVar_SwarmAmmoCost1, "SwarmAmmoCost1", 0, " Swarm Ammo Cost 1 ", 7)
INIT_GVAR_INT(Init_GVar_SwarmAmmoCost2, &GVar_SwarmAmmoCost2, "SwarmAmmoCost2", 0, " Swarm Ammo Cost 2 ", 13)
INIT_GVAR_INT(Init_GVar_SwarmAmmoCost3, &GVar_SwarmAmmoCost3, "SwarmAmmoCost3", 0, " Swarm Ammo Cost 3 ", 18)
INIT_GVAR_INT(Init_GVar_SwarmAmmoCost4, &GVar_SwarmAmmoCost4, "SwarmAmmoCost4", 0, " Swarm Ammo Cost 4 ", 22)
INIT_GVAR_INT(Init_GVar_SwarmNumber1, &GVar_SwarmNumber1, "SwarmNumber1", 0, " Swarm Number 1 ", 7)
FINI_GVAR_STRING(Fini_GVar_SwarmEngineSound, &GVar_SwarmEngineSound)
INIT_GVAR_STRING(Init_GVar_SwarmEngineSound, Fini_GVar_SwarmEngineSound, &GVar_SwarmEngineSound, "SwarmEngineSound", 0, "Swarm Engine sound name", "SwarmEngine")
INIT_GVAR_FLOAT(Init_GVar_SwarmEngineSoundVol, &GVar_SwarmEngineSoundVol, "SwarmEngineSoundVol", 0, "Swarm Engine sound volume", 0.75f)
INIT_GVAR_FLOAT(Init_GVar_SwarmEngineSoundAttIni, &GVar_SwarmEngineSoundAttIni, "SwarmEngineSoundAttIni", 0, "Swarm Engine sound attenuation ini", 0.0f)
INIT_GVAR_FLOAT(Init_GVar_SwarmEngineSoundAttEnd, &GVar_SwarmEngineSoundAttEnd, "SwarmEngineSoundAttEnd", 0, "Swarm Engine sound attenuation end", 0.0f)
INIT_GVAR_FLOAT(Init_GVar_SwarmEngineSoundDoppler, &GVar_SwarmEngineSoundDoppler, "SwarmEngineSoundDoppler", 0, "Swarm Engine sound doppler", 1.0f)
FINI_GVAR_STRING(Fini_GVar_SwarmBeeSound, &GVar_SwarmBeeSound)
INIT_GVAR_STRING(Init_GVar_SwarmBeeSound, Fini_GVar_SwarmBeeSound, &GVar_SwarmBeeSound, "SwarmBeeSound", 0, "Swarm Bee Death sound name", "SwarmBang")
INIT_GVAR_FLOAT(Init_GVar_SwarmBeeSoundVol, &GVar_SwarmBeeSoundVol, "SwarmBeeSoundVol", 0, "Swarm Bee Death sound volume", 0.5f)
INIT_GVAR_FLOAT(Init_GVar_SwarmBeeSoundAttIni, &GVar_SwarmBeeSoundAttIni, "SwarmBeeSoundAttIni", 0, "Swarm Bee sound attenuation ini", 0.0f)
INIT_GVAR_FLOAT(Init_GVar_SwarmBeeSoundAttEnd, &GVar_SwarmBeeSoundAttEnd, "SwarmBeeSoundAttEnd", 0, "Swarm Bee sound attenuation end", 0.0f)
FINI_GVAR_STRING(Fini_GVar_SwarmHitSound, &GVar_SwarmHitSound)
INIT_GVAR_STRING(Init_GVar_SwarmHitSound, Fini_GVar_SwarmHitSound, &GVar_SwarmHitSound, "SwarmHitSound", 0, "Swarm Hit sound name", "SwarmBoom")
INIT_GVAR_FLOAT(Init_GVar_SwarmHitSoundVol, &GVar_SwarmHitSoundVol, "SwarmHitSoundVol", 0, "Swarm Hit sound volume", 1.0f)
INIT_GVAR_FLOAT(Init_GVar_SwarmHitSoundAttIni, &GVar_SwarmHitSoundAttIni, "SwarmHitSoundAttIni", 0, "Swarm Hit sound attenuation ini", 0.0f)
INIT_GVAR_FLOAT(Init_GVar_SwarmHitSoundAttEnd, &GVar_SwarmHitSoundAttEnd, "SwarmHitSoundAttEnd", 0, "Swarm Hit sound attenuation end", 0.0f)
INIT_GVAR_FLOAT(Init_GVar_TeslaMinAngRange, &GVar_TeslaMinAngRange, "TeslaMinAngRange", 0, "The minimun angle range (no hit case at tesla)", 3.0f)
INIT_GVAR_FLOAT(Init_GVar_TeslaMinAngRangePad, &GVar_TeslaMinAngRangePad, "TeslaMinAngRangePad", 0, "The minimun angle range (no hit case at tesla)  Pad Controller", 10.0f)
INIT_GVAR_FLOAT(Init_GVar_TeslaDownRangeTime, &GVar_TeslaDownRangeTime, "TeslaDownRangeTime", 0, "Time to the max to the min ang range if not hit (tesla)", 0.5f)
INIT_GVAR_FLOAT(Init_GVar_TeslaDownRangeTimePad, &GVar_TeslaDownRangeTimePad, "TeslaDownRangeTimePad", 0, "Time to the max to the min ang range if not hit (tesla) Pad Controller", 2.0f)
INIT_GVAR_FLOAT(Init_GVar_TeslaMinAngRangeUpg, &GVar_TeslaMinAngRangeUpg, "TeslaMinAngRangeUpg", 0, "The minimun angle range with upgrade (no hit case at tesla)", 10.0f)
INIT_GVAR_FLOAT(Init_GVar_TeslaMinAngRangeUpgPad, &GVar_TeslaMinAngRangeUpgPad, "TeslaMinAngRangeUpgPad", 0, "The minimun angle range with upgrade (no hit case at tesla) Pad Controller", 17.0f)
INIT_NATIVE_FUNC(Init_NativeFunc_SwarmPress, &NativeFunc_SwarmPress, "SwarmPress", Swarm_AimMode3Native)
INIT_NATIVE_FUNC(Init_NativeFunc_SwarmRelease, &NativeFunc_SwarmRelease, "SwarmRelease", Swarm_AimMode2Native)
INIT_NATIVE_FUNC(Init_NativeFunc_SwarmOnEnd, &NativeFunc_SwarmOnEnd, "SwarmOnEnd", Swarm_AimOffNative)
INIT_NATIVE_FUNC(Init_NativeFunc_SwarmOnStart, &NativeFunc_SwarmOnStart, "SwarmOnStart", Swarm_AimOnNative)
INIT_NATIVE_FUNC(Init_NativeFunc_SwarmHit, &NativeFunc_SwarmHit, "SwarmHit", Swarm_HitFXNative)
INIT_NATIVE_FUNC(Init_NativeFunc_SwarmServerHit, &NativeFunc_SwarmServerHit, "SwarmServerHit", Swarm_ServerHitNative)
INIT_NATIVE_FUNC(Init_NativeFunc_SwarmCreate, &NativeFunc_SwarmCreate, "SwarmCreate", Swarm_NetCreateNative)
INIT_NATIVE_FUNC(Init_NativeFunc_SwarmSetUp, &NativeFunc_SwarmSetUp, "SwarmSetUp", Swarm_NetSetUpNative)
INIT_NATIVE_FUNC(Init_NativeFunc_SwarmClientFire, &NativeFunc_SwarmClientFire, "SwarmClientFire", Swarm_ShootNative)
INIT_NATIVE_FUNC(Init_NativeFunc_SwarmFire, &NativeFunc_SwarmFire, "SwarmFire", Swarm_FireNative)
INIT_NATIVE_FUNC(Init_NativeFunc_SwarmFX, &NativeFunc_SwarmFX, "SwarmFX", Swarm_HudNative)
