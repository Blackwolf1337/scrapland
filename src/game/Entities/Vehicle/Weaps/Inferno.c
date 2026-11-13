/*
 * Entities/Vehicle/Weaps/Inferno.cpp of the game: the Inferno missile, see include/scrap/weaps2.h. The missile
 * carries one bee per mounted Inferno bay; it explodes in a sphere whose range grows with the bees, hurts and pushes
 * the entities in it (InfernoXplosionCall) and shakes the cameras of the players near it.
 */
#include <string.h>

#include "scrap/crt.h"
#include "scrap/play.h"
#include "scrap/weaps2.h"
#include "scrap/staticinit.h"

#define g_InfernoEngineSoundVol (GVar_InfernoEngineSoundVol.value.f)
#define g_InfernoTimeDelay (GVar_InfernoTimeDelay.value.f)
#define g_InfernoUpTopSpeed (GVar_InfernoUpTopSpeed.value.f)
#define g_InfernoLifeTime (GVar_InfernoLifeTime.value.f)
#define g_InfernoEngineSoundAttIni (GVar_InfernoEngineSoundAttIni.value.f)
#define g_InfernoTurnSpeed (GVar_InfernoTurnSpeed.value.f)
#define g_InfernoMaxDamage (GVar_InfernoMaxDamage.value.f)
#define g_InfernoHitSoundVol (GVar_InfernoHitSoundVol.value.f)
#define g_InfernoHitSoundAttEndRadius (GVar_InfernoHitSoundAttEndRadius.value.f)
#define g_InfernoEngineSoundDoppler (GVar_InfernoEngineSoundDoppler.value.f)
#define g_InfernoBeesSpeed (GVar_InfernoBeesSpeed.value.f)
#define g_InfernoUpgFireFactor (GVar_InfernoUpgFireFactor.value.f)
#define g_InfernoInvUpgExtraDamage (GVar_InfernoInvUpgExtraDamage.value.f)
#define g_InfernoHitForce (GVar_InfernoHitForce.value.f)
#define g_InfernoArmor (GVar_InfernoArmor.value.i)
#define g_InfernoUpgFireTime (GVar_InfernoUpgFireTime.value.f)
#define g_InfernoHitSound (GVar_InfernoHitSound.value.s)
#define g_InfernoMinDamage (GVar_InfernoMinDamage.value.f)
#define g_InfernoEngineSound (GVar_InfernoEngineSound.value.s)
#define g_InfernoNormSpeed (GVar_InfernoNormSpeed.value.f)
#define g_InfernoXplosionCall (NativeFunc_InfernoXplosionCall.func)	/* the function of the native "InfernoXplosionCall" */
#define g_InfernoMissileFile (GVar_InfernoMissileFile.value.s)
#define g_FXInfernoExplosionMaxDist (GVar_FXInfernoExplosionMaxDist.value.f)
#define g_InfernoHitSoundAttIniRadius (GVar_InfernoHitSoundAttIniRadius.value.f)
#define g_InfernoEngineSoundAttEnd (GVar_InfernoEngineSoundAttEnd.value.f)
#define g_InfernoExplosionFile (GVar_InfernoExplosionFile.value.s)
static cPy_NativeFunc NativeFunc_InfernoXplosionCall;
static cGVar GVar_InfernoUpgFireTime;
static cGVar GVar_InfernoUpgFireFactor;
static cGVar GVar_InfernoUpTopSpeed;
static cGVar GVar_InfernoTurnSpeed;
static cGVar GVar_InfernoTimeDelay;
static cGVar GVar_InfernoNormSpeed;
static cGVar GVar_InfernoMissileFile;
static cGVar GVar_InfernoMinDamage;
static cGVar GVar_InfernoMaxDamage;
static cGVar GVar_InfernoLifeTime;
static cGVar GVar_InfernoInvUpgExtraDamage;
static cGVar GVar_InfernoHitSoundVol;
static cGVar GVar_InfernoHitSoundAttIniRadius;
static cGVar GVar_InfernoHitSoundAttEndRadius;
static cGVar GVar_InfernoHitSound;
static cGVar GVar_InfernoHitForce;
static cGVar GVar_InfernoExplosionFile;
static cGVar GVar_InfernoEngineSoundVol;
static cGVar GVar_InfernoEngineSoundDoppler;
static cGVar GVar_InfernoEngineSoundAttIni;
static cGVar GVar_InfernoEngineSoundAttEnd;
static cGVar GVar_InfernoEngineSound;
static cGVar GVar_InfernoBeesSpeed;
static cGVar GVar_InfernoArmor;
static cGVar GVar_FXInfernoExplosionMaxDist;

static const char *const inferno_types[] = {
	"Misil Inferno Uranio\n", "Misil Inferno Hidrogeno\n", "Misil Inferno Napalm\n"
};

/* the reload sound, the delay of the next shot and the shoot effect at every mounted bay */
void Inferno_Shoot(const char *name) {
	cEntity *v = Weap_Vehicle(name);
	if (cTargetLock_GetAmmo(&Veh(v)->weapons, g_InfernoIDAmmo) >= (float)g_InfernoAmmoCost) {
		Weap_PlaySound(v, 0x15);
		Veh(v)->weapons.fireStamp = -g_InfernoTimeDelay;
		for (int i = 0; i < 4; i++) {
			if (cTargetLock_HasSlot(&Veh(v)->weapons, g_InfernoIDWeap, i)) {
				cVehicleEffects_Shoot(Veh_Effects(v), g_InfernoIDWeap, i);
			}
		}
	}
}

/* name, position, orientation */
static void Inferno_NetCreateNative(cAbstractFile *f) {
	char name[0x80];
	vec3_t pos, dir;

	cAbstractFile_ReadString(f, name, 0x80);
	Weap_ReadVec(f, &pos);
	Weap_ReadVec(f, &dir);
	Inferno_NetCreate(name, pos, dir);
}

static void Inferno_HitNative(cAbstractFile *f);
cEntity *Inferno_NetCreate(const char *name, vec3_t pos, vec3_t dir) {
	cEntity *m = cWorld_CreateEntity(g_pWorld, name, pos.x, pos.y, pos.z, TYEN_MISSILE);
	CHECK(m);
	CHECK(Entity_IsType(m, TYEN_MISSILE));
	cMissileEntity_SetHiddenSwarm((cMissileEntity *)m, 0);
	cMissileEntity_SetHideMain((cMissileEntity *)m, 1);
	Obj_SetAng(m, &dir);
	cMissileEntity_SetSwarmSpeed((cMissileEntity *)m, g_InfernoBeesSpeed);
	cMissileEntity_SetLifeTime((cMissileEntity *)m, g_InfernoLifeTime);
	cPy_Function_SetNative(&Missile(m)->onHit, (nativefunc_t)Inferno_HitNative);
	cEntity_SetNetResource(m, "Inferno");
	Missile(m)->weap = MISSILE_WEAP_INFERNO;
	return m;
}

/*
 * the cursor of the HUD item: it turns and shrinks for a new target (time 1 to 0), then shows the level of the
 * weapon; shared with the Swarm (its own buffers and levels). Ship name, player, item name.
 */
void Weap_UpdateHud(cAbstractFile *f, char (*targets)[0x80], float *times, int levelRot, int levelSize) {
	char name[0x80], itemName[0x80];
	int player;

	cAbstractFile_ReadString(f, name, 0x80);
	VFile_Read(f, &player, 4);
	cAbstractFile_ReadString(f, itemName, 0x80);
	cEntity *v = Weap_Vehicle(name);
	cCannonScorer *item = (cCannonScorer *)cScorer_FindItem(Scorer_Get(player), itemName);
	if (!item) {
		return;
	}
	float dt = Scorer_Get(player)->frameTime;
	if (strcmp(Nto1Node_TargetName(&Veh(v)->autoAim.target), targets[player])) {
		Str_Copy(targets[player], Nto1Node_TargetName(&Veh(v)->autoAim.target), 0x80);
		times[player] = Veh(v)->autoAim.target.target ? 1.0f : 0.0f;
	}
	float t = times[player];
	if (0.0f >= t) {
		times[player] = t - dt;
		int level = cWeaponManager_GetUpgrade(&Veh(v)->weapons, Veh(v)->weapons.curWeap);
		if (Veh(v)->autoAim.target.target && (level == levelRot || level == levelSize)) {
			if (-0.5f >= times[player]) {
				times[player] = 0.0f;
			}
			float x = (float)__builtin_fabs((double)(times[player] * 4.0f + 1.0f));
			uint32_t a = (uint32_t)(-1 - (int)(x * 64.0f));
			item->color = ((((a << 8) | g_CannonScorerColorRed) << 8 | g_CannonScorerColorGreen) << 8) |
				g_CannonScorerColorBlue;
			if (level == levelRot) {
				item->size[0] = 20.0f;
				item->size[1] = 20.0f;
				item->angle = (1.0f - x) * 3.14159274f * 0.25f;
			} else {
				float s = (1.0f - x) * 7.0f + 20.0f;
				item->size[0] = s;
				item->size[1] = s;
				item->angle = 0.0f;
			}
			return;
		}
		item->size[0] = 20.0f;
		item->size[1] = 20.0f;
		item->color = (((g_CannonScorerColorRed | 0xffffbf00u) << 8 | g_CannonScorerColorGreen) << 8) |
			g_CannonScorerColorBlue;
		item->angle = 0.0f;
		return;
	}
	t = t - dt * 3.0f;
	times[player] = t;
	item->angle = t * 1.57079637f;
	float s = times[player] * 50.0f + 20.0f;
	item->size[0] = s;
	item->size[1] = s;
	uint32_t a = (uint32_t)(-64 - (int)(times[player] * 192.0f));
	item->color = ((((a << 8) | g_CannonScorerColorRed) << 8 | g_CannonScorerColorGreen) << 8) |
		g_CannonScorerColorBlue;
}

static char g_InfernoHudTarget[4][0x80];
static float g_InfernoHudTime[4];

static void Inferno_HudNative(cAbstractFile *f) {
	Weap_UpdateHud(f, g_InfernoHudTarget, g_InfernoHudTime, 11, 12);
}

/* the ship shoots: a missile with one bee per Inferno ammo cost (up to the bays) */
static void Inferno_OnDamageNative(cAbstractFile *f);
static void Inferno_OnDeathNative(cAbstractFile *f);
static void Inferno_ServerHitNative(cAbstractFile *f);
static void Inferno_FireNative(cAbstractFile *f) {
	char name[0x80], obj[0x80];
	vec3_t pos, ang, vel;

	cAbstractFile_ReadString(f, name, 0x80);
	Inferno_Shoot(name);
	cEntity *v = Weap_Vehicle(name);
	float ammo = cTargetLock_GetAmmo(&Veh(v)->weapons, g_InfernoIDAmmo);
	if (!(ammo >= (float)g_InfernoAmmoCost)) {
		return;
	}
	Entity_GetPos(v, &pos);
	Obj_GetAng(v, &ang);
	Obj_GetVel(v, &vel);
	Net_GetObjName(obj);
	cEntity *m = Inferno_NetCreate(obj, pos, ang);
	if (!m) {
		return;
	}
	Controls_RumbleA(&Veh(v)->controls, 1.0f, 0.3f);
	Controls_RumbleB(&Veh(v)->controls, 1.0f, 0.5f);
	cPy_Function_SetNative(&Missile(m)->onHit, (nativefunc_t)Inferno_ServerHitNative);
	cPy_Function_SetNative(&WLife(m)->onDeath, (nativefunc_t)Inferno_OnDeathNative);
	cPy_Function_SetNative(&WLife(m)->onDamage, (nativefunc_t)Inferno_OnDamageNative);
	cMissileEntity_SetBeesHit((cMissileEntity *)m, 1);
	if (!WLife(m)->god) {
		WLife(m)->life = g_InfernoArmor;
	}
	cPrimitive_SetSize(Mis_Size(m), 50.0f);
	cMissileEntity_SetParent((cMissileEntity *)m, name);
	cMissileEntity_SetTarget((cMissileEntity *)m, Nto1Node_TargetName(&Veh(v)->autoAim.target));
	Inferno_NetSetUp(m->name);
	int n = (int)ammo / g_InfernoAmmoCost;
	if (Missile(m)->swarmCount > n) {
		cMissileEntity_SetSwarm(Missile(m), n);
	}
	cPrimitive_SetSize(Mis_Size(m), Missile(m)->topRadius * 0.6f);
	if (!WLife(m)->god) {
		WLife(m)->life = Missile(m)->swarmCount * g_InfernoArmor;
	}
	cMissileEntity_SetLifeTime((cMissileEntity *)m, Missile(m)->swarmCount ? g_InfernoLifeTime - 0.2f : 0.1f);
	cWeaponManager_SetAmmo(&Veh(v)->weapons, g_InfernoIDAmmo,
		(float)(int)ammo - (float)(Missile(m)->swarmCount * g_InfernoAmmoCost));
}

/* missile name, two names not used */
static cEntity *Inferno_Hit(const char *name);
static void Inferno_HitNative(cAbstractFile *f) {
	char name[0x80], a[0x80], b[0x80];

	cAbstractFile_ReadString(f, name, 0x80);
	cAbstractFile_ReadString(f, a, 0x80);
	cAbstractFile_ReadString(f, b, 0x80);
	Inferno_Hit(name);
}

/* the explosion effect, the camera shake and flash of the players in range, the hit sound; returns the missile */
static cEntity *Inferno_Hit(const char *name) {
	vec3_t pos, op, tmp, rnd, nrm;

	cEntity *m = Weap_Missile(name);
	Entity_GetPos(m, &pos);
	float range = (float)Missile(m)->swarmCount * g_InfernoRangeBay + g_InfernoRangeMin;
	uint32_t type = (uint32_t)Missile(m)->level;
	Con_Print(0x606060, type < 3 ? inferno_types[type] : "Misil Inferno Desconocido\n", 0);
	if (!FX_IsVisible(pos.x, pos.y, pos.z, range, g_FXInfernoExplosionMaxDist, NULL, 1)) {
		return m;
	}
	cDebrisSys *sys = cWorld_GetDebrisSys(g_pWorld, g_InfernoExplosionFile);
	cDebris *d = cDebrisSys_NewElement(sys);
	CGameModel_SetPos(Debris_Model(d), &pos);
	d->life = g_FXInfernoExplosionTime;
	cDebrisSys_Drop(sys, d);
	cObjElementFXInfernoExplosion *fx = Debris_Element(d);
	if (fx) {
		if (!fx->on) {
			fx->on = 1;
		}
		cObjElementFXInfernoExplosion_Explode(fx, pos.x, pos.y, pos.z, range, Missile(m)->swarmCount, 0);
	}
	int i = 0;
	do {
		cEntity *o = Weap_Owner(g_pWorld, i);
		if (o && Obj(o)->cam) {
			byte hit = Engine_TestLine(&pos, Entity_GetPos(o, &tmp), NULL, NULL);
			if (Obj(o)->cam && !hit) {
				const vec3_t *p = Entity_GetPos(o, &op);
				float dy = p->y - pos.y;
				float dx = p->x - pos.x;
				float dz = p->z - pos.z;
				float dist = (float)__builtin_sqrt((double)(dy * dy + dx * dx + dz * dz));
				if (1.0f > dist) {
					double r0 = (double)(crt_rand() & 0x7fff) * RAND_SCALE;
					double r1 = (double)(crt_rand() & 0x7fff) * RAND_SCALE;
					double r2 = (double)(crt_rand() & 0x7fff) * RAND_SCALE;
					rnd.x = (float)((r2 + r2) - 1.0);
					rnd.y = (float)((r1 + r1) - 1.0);
					rnd.z = (float)((r0 + r0) - 1.0);
					Vec3_NormalizeTo(&nrm, &rnd);
					dist = 1.0f;
				}
				dist = Float_MaxSS(1.0f, dist - WLife(o)->prim.radius);
				if (range >= dist) {
					float w = (range - dist) / range;
					w = w * w * (2.0f - w);
					cScorer *sc = NULL;
					int found = 1;
					if (g_SplitScreenGame) {
						int k;
						for (k = 0; k < 4; k++) {
							const char *cam = Obj(o)->cam->name;
							if (!strcmp(cam, cWorld_GetCamName(g_pWorld, k))) {
								break;
							}
						}
						found = k < 4;
						if (found) {
							sc = Scorer_Get(k);
						}
					} else {
						sc = Scorer_Get(0);
					}
					if (found && sc) {
						uint32_t a = (uint32_t)(-64 - (int)(w * -63.0f));
						ScorerFade_Start(&sc->fade, (a << 24) | 0xffbe3c, 0xffffff, g_InfernoLifeTime * w * 0.5f);
					}
					cEntity *cam = Obj(o)->cam;
					if (cam) {
						Cam_Shake(cam) = w * 0x1.c0f2eep-5f;
						Cam_ShakeTime(cam) = g_InfernoLifeTime * w * 0.25f;
					}
				}
			}
		}
	} while (++i < (g_SplitScreenGame != 0) + 1);
	cSoundDesc desc;
	cSoundDesc_Init(&desc);
	desc.param3 = 0.0f;
	desc.param1 = g_InfernoHitSoundAttIniRadius * range;
	desc.param2 = g_InfernoHitSoundAttEndRadius * range;
	cSoundDesc_SetPos(&desc, &pos);
	desc.volume += Snd_Volume(g_InfernoHitSoundVol);
	SoundSys_PlaySound(g_pSound, g_InfernoHitSound, &desc, NULL);
	return m;
}

/* missile name, attacker name, damage: one bee lost per InfernoArmor */
static void Inferno_OnDamageNative(cAbstractFile *f) {
	char name[0x80], other[0x80];
	int damage;

	cAbstractFile_ReadString(f, name, 0x80);
	cAbstractFile_ReadString(f, other, 0x80);
	VFile_Read(f, &damage, 4);
	cEntity *m = Weap_Missile(name);
	int bees;
	if (Ent_Active(m) && (bees = Missile(m)->swarmCount) > 0) {
		int armor = g_InfernoArmor;
		if (damage >= armor) {
			int n = damage / armor;
			cMissileEntity_Hit(Missile(m), n > bees ? bees : n, 0);
		}
	}
}

static void Inferno_Explode(const char *name);
static void Inferno_OnDeathNative(cAbstractFile *f) {
	char name[0x80], other[0x80];

	cAbstractFile_ReadString(f, name, 0x80);
	cAbstractFile_ReadString(f, other, 0x80);
	cEntity *m = Weap_Missile(name);
	if (Ent_Active(m)) {
		Entity_SetActive(m, 0);
		Inferno_Explode(name);
		cMissileEntity_SetSwarm(Missile(m), 0);
	}
}

static void Inferno_AimOffNative(cAbstractFile *f) {
	Weap_SetAimMode(f, 0);
}

/* missile name, a number not used: the speed back towards InfernoNormSpeed */
static void Inferno_SlowNative(cAbstractFile *f) {
	char name[0x80];
	int unused;

	cAbstractFile_ReadString(f, name, 0x80);
	VFile_Read(f, &unused, 4);
	cEntity *m = Weap_Missile(name);
	float speed = Missile(m)->speed;
	float norm = g_InfernoNormSpeed;
	if (speed > norm) {
		cMissileEntity_SetSpeed((cMissileEntity *)m, (speed - norm) * 0.9f + norm);
	}
}

static void Inferno_AimOnNative(cAbstractFile *f) {
	cEntity *v = Weap_SetAimMode(f, 2);
	cAutoAim_SetRange(Veh_Aim(v), g_InfernoDistLimit);
	cAutoAim_SetAngle(Veh_Aim(v), 0x1.1df46ap-3f);
}

static void Inferno_AimMode3Native(cAbstractFile *f) {
	Weap_SetAimMode(f, 3);
}

static void Inferno_AimMode2Native(cAbstractFile *f) {
	Weap_SetAimMode(f, 2);
}

static void Inferno_ServerHitNative(cAbstractFile *f) {
	char name[0x80];

	cAbstractFile_ReadString(f, name, 0x80);
	Inferno_Explode(name);
}

static cEntity *g_InfernoOwner;
static float g_InfernoRange;
static cEntity *g_InfernoMissile;
static vec3_t g_InfernoPos;

/* the explosion: the effect, then (not on clients) InfernoXplosionCall on the entities in the sphere */
static void Inferno_Explode(const char *name) {
	vec3_t tmp;

	cEntity *m = g_InfernoMissile = Inferno_Hit(name);
	if (!m) {
		return;
	}
	const vec3_t *p = Entity_GetPos(m, &tmp);
	g_InfernoPos.x = p->x;
	g_InfernoPos.y = p->y;
	g_InfernoPos.z = p->z;
	m = g_InfernoMissile;
	const char *owner = Missile(m)->parent;
	g_InfernoRange = (float)Missile(m)->swarmCount * g_InfernoRangeBay + g_InfernoRangeMin;
	if (*owner) {
		HashTable *t = &g_pWorld->entities;
		HashNode *node = HashTable_Find(t, owner, HashTable_Hash(t, owner));
		cEntity *e = node ? node->value : NULL;
		g_InfernoOwner = e;
		if (!e || !Entity_IsType(e, TYEN_VEHICLE)) {
			g_InfernoOwner = NULL;
		}
	} else {
		g_InfernoOwner = NULL;
	}
	if (!(g_pApp->flags & APPF_CLIENT)) {
		cPy_Function_SetNative(g_InfernoSphereFunc, g_InfernoXplosionCall);
		cWorld_SphereCallF(g_pWorld, 8, &g_InfernoPos, g_InfernoRange, g_InfernoSphereFunc, 0);
	}
}

static void Inferno_NetSetUpNative(cAbstractFile *f) {
	char name[0x80];

	cAbstractFile_ReadString(f, name, 0x80);
	Inferno_NetSetUp(name);
}

/* the model, speeds, bees (one per mounted bay of the ship), the upgrade type and the engine sound of a missile */
void Inferno_NetSetUp(const char *name) {
	vec3_t tmp, ang, vel;
	matrix_t mtx;

	cEntity *m = Weap_Missile(name);
	Entity_GetPos(m, &tmp);
	Entity_SetModel(m, g_InfernoMissileFile, 1.0f, 1.0f, 1.0f, 0);
	cMissileEntity_SetRadius((cMissileEntity *)m, 100.0f);
	cMissileEntity_SetTopRadius((cMissileEntity *)m, 400.0f);
	cMissileEntity_SetRadiusGrow((cMissileEntity *)m, 200.0f);
	cMissileEntity_SetSpeed((cMissileEntity *)m, g_InfernoNormSpeed);
	cMissileEntity_SetTurnSpeed((cMissileEntity *)m, (float)(((double)g_InfernoTurnSpeed * WEAP_PI) / 180.0));
	cMissileEntity_SetBeesGather((cMissileEntity *)m, 0);
	cEntity *v = Weap_FindVehicle(Missile(m)->parent);
	if (v) {
		float top = g_InfernoUpTopSpeed;
		if (Veh(v)->speed > g_InfernoNormSpeed - top) {
			cMissileEntity_SetSpeed((cMissileEntity *)m, top + Veh(v)->speed);
		}
		cTargetLock *wm = &Veh(v)->weapons;
		for (int i = 0; i < 4; i++) {
			if (!cTargetLock_HasSlot(wm, g_InfernoIDWeap, i)) {
				continue;
			}
			Weap_DropAtNode(v, Veh(v)->weapons.nodes[g_InfernoIDAmmo][i], &mtx, &ang, &vel);
			cMissileEntity_AddToSwarm(Missile(m), 1);
			if (i == 0 && Missile(m)->swarm && Debris_Element(Missile(m)->swarm)) {
				cObjElementFXInfernoMissile_SetFlare(Debris_Element(Missile(m)->swarm), 1);
			}
		}
		int level = cWeaponManager_GetUpgrade(wm, g_InfernoIDWeap);
		Missile(m)->level = level;
		if ((g_SplitScreenGame || g_pApp->flags) && (level == 11 || level == 12)) {
			int bees = Missile(m)->swarmCount;
			Missile(m)->level = level - 10;
			float left = cWeaponManager_GetUAmmo(wm, g_InfernoIDWeap) - (float)(bees * 51);
			cWeaponManager_SetUAmmo(wm, g_InfernoIDWeap, left);
			if (0.0f >= left) {
				cPy_Function_AddParamI(g_RestoreUpgradeFunc, Missile(m)->level + 10);
				cPy_Function_Execute(g_RestoreUpgradeFunc, 0);
				cWeaponManager_SetUpgrade(wm, g_InfernoIDWeap, 0);
			}
		}
	}
	cMissileEntity_SetSwarm(Missile(m), cMissileEntity_GetSwarmCount((cMissileEntity *)m));
	WithLife_SetMass(m, 6000.0f);
	Missile(m)->fallOut = 1;
	Weap_EngineSound(m, g_InfernoEngineSound, g_InfernoEngineSoundVol, g_InfernoEngineSoundAttIni,
		g_InfernoEngineSoundAttEnd, g_InfernoEngineSoundDoppler);
}

/* the state of the module */
static char g_InfernoHitName[128];

/* InfernoXplosionCall, for each entity in the sphere of the explosion: damage, push and burn by the distance */
static void Inferno_SphereHit(cAbstractFile *f) {
	vec3_t tmp, rnd, nrm, vel;

	cAbstractFile_ReadString(f, g_InfernoHitName, 0x80);
	if (!*g_InfernoHitName) {
		return;
	}
	HashTable *t = &g_pWorld->entities;
	HashNode *node = HashTable_Find(t, g_InfernoHitName, HashTable_Hash(t, g_InfernoHitName));
	if (!node) {
		return;
	}
	cEntity *e = node->value;
	if (!e || !Entity_IsType(e, TYEN_WITH_LIFE)) {
		return;
	}
	int life = WLife(e)->life;
	const vec3_t *p = Entity_GetPos(e, &tmp);
	float dy = p->y - g_InfernoPos.y;
	float dx = p->x - g_InfernoPos.x;
	float dz = p->z - g_InfernoPos.z;
	float dist = (float)__builtin_sqrt((double)(dy * dy + dx * dx + dz * dz));
	if (dist != 0.0f) {
		float inv = 1.0f / dist;
		dx *= inv;
		dy *= inv;
		dz *= inv;
	}
	if (1.0f > dist) {
		double r0 = (double)(crt_rand() & 0x7fff) * RAND_SCALE;
		double r1 = (double)(crt_rand() & 0x7fff) * RAND_SCALE;
		double r2 = (double)(crt_rand() & 0x7fff) * RAND_SCALE;
		rnd.x = (float)((r2 + r2) - 1.0);
		rnd.y = (float)((r1 + r1) - 1.0);
		rnd.z = (float)((r0 + r0) - 1.0);
		const vec3_t *n = Vec3_NormalizeTo(&nrm, &rnd);
		dx = n->x;
		dy = n->y;
		dz = n->z;
		dist = 1.0f;
	}
	float d = Float_MaxSS(1.0f, dist - WLife(e)->prim.radius);
	float k = (g_InfernoRange - d) / g_InfernoRange;
	if (Missile(g_InfernoMissile)->level == 1) {
		float inv = g_InfernoInvUpgExtraDamage;
		if (k > inv) {
			k = 1.0f;
		} else {
			k *= 1.0f / inv;
		}
	}
	float w = k * k * (2.0f - k);
	if (!WLife(e)->god && Rep_Hurt(g_InfernoOwner, e, "Inferno")) {
		WLife(e)->life -= (int)((g_InfernoMaxDamage - g_InfernoMinDamage) * w);
	}
	byte isWalk = Entity_IsType(e, TYEN_WALKCHAR);
	byte isCar = Entity_IsType(e, TYEN_VEHICLE);
	if (isWalk || isCar) {
		if (g_InfernoOwner && life > WLife(e)->life) {
			cVehicleAttack_ShotAt(Veh_HitInfo(g_InfernoOwner), g_InfernoHitName, 0);
		}
	}
	if (!isWalk) {
		float px = dx * w;
		float py = dy * w;
		float pz = dz * w;
		float force = g_InfernoHitForce * 1000.0f / WLife(e)->mass;
		px *= force;
		py *= force;
		pz *= force;
		const vec3_t *ov = Obj_GetVel(e, &tmp);
		vel.x = px + ov->x;
		vel.y = ov->y + py;
		vel.z = ov->z + pz;
		Obj_SetVel(e, &vel);
	}
	if (isCar && Veh(e)->netCtrl) {
		Veh(e)->netCtrl->hit = 2;
	}
	if (WLife(e)->life > 0 && Missile(g_InfernoMissile)->level == 2) {
		WLife(e)->fireRemain = g_InfernoUpgFireTime;
		WLife(e)->fireAtt = 0.0f;
		WLife(e)->fireDelay = ((1.0f - w) + 0.1f) / g_InfernoUpgFireFactor;
		cWithLifeEntity_SetBurner(e, g_InfernoOwner);
	}
}

cGVar GVar_CannonScorerColorRed;
cGVar GVar_CannonScorerColorBlue;
cGVar GVar_CannonScorerColorGreen;
static cGVar GVar_InfernoEngineSoundVol;
cGVar GVar_InfernoRangeMin;
static cGVar GVar_InfernoTimeDelay;
cGVar GVar_InfernoAmmoCost;
static cGVar GVar_InfernoUpTopSpeed;
cGVar GVar_InfernoIDAmmo;
static cGVar GVar_InfernoLifeTime;
static cGVar GVar_InfernoEngineSoundAttIni;
static cGVar GVar_InfernoTurnSpeed;
static cGVar GVar_InfernoMaxDamage;
static cGVar GVar_InfernoHitSoundVol;
static cGVar GVar_InfernoHitSoundAttEndRadius;
static cGVar GVar_InfernoEngineSoundDoppler;
static cGVar GVar_InfernoBeesSpeed;
static cGVar GVar_InfernoUpgFireFactor;
static cGVar GVar_InfernoInvUpgExtraDamage;
static cGVar GVar_InfernoHitForce;
cGVar GVar_InfernoRangeBay;
static cGVar GVar_InfernoArmor;
static cGVar GVar_InfernoUpgFireTime;
static cGVar GVar_InfernoHitSound;
static cGVar GVar_InfernoMinDamage;
static cGVar GVar_InfernoEngineSound;
static cGVar GVar_InfernoNormSpeed;
static cPy_NativeFunc NativeFunc_InfernoXplosionCall;
static cGVar GVar_InfernoMissileFile;
static cGVar GVar_FXInfernoExplosionMaxDist;
static cGVar GVar_InfernoHitSoundAttIniRadius;
static cGVar GVar_InfernoEngineSoundAttEnd;
static cGVar GVar_InfernoExplosionFile;
static cPy_NativeFunc NativeFunc_InfernoPress;
static cPy_NativeFunc NativeFunc_InfernoRelease;
static cPy_NativeFunc NativeFunc_InfernoOnEnd;
static cPy_NativeFunc NativeFunc_InfernoOnStart;
static cPy_NativeFunc NativeFunc_InfernoHit;
static cPy_NativeFunc NativeFunc_InfernoServerHit;
static cPy_NativeFunc NativeFunc_InfernoOnObjSlot;
static cPy_NativeFunc NativeFunc_InfernoCreate;
static cPy_NativeFunc NativeFunc_InfernoSetUp;
static cPy_NativeFunc NativeFunc_InfernoFire;
static cPy_NativeFunc NativeFunc_InfernoFX;

/* the dynamic initializers of the globals here, run in the order of src/standalone/crtxc.inc */
INIT_GVAR_INT(Init_GVar_InfernoIDAmmo, &GVar_InfernoIDAmmo, "InfernoIDAmmo", 0, "The default ammo idx for the Inferno", 1)
INIT_GVAR_FLOAT(Init_GVar_InfernoLifeTime, &GVar_InfernoLifeTime, "InfernoLifeTime", 0, "The maximun lifetime that have the Inferno", 6.5f)
INIT_GVAR_INT(Init_GVar_InfernoArmor, &GVar_InfernoArmor, "InfernoArmor", 0, "The default armor for one swarm missile", 4)
FINI_GVAR_STRING(Fini_GVar_InfernoMissileFile, &GVar_InfernoMissileFile)
INIT_GVAR_STRING(Init_GVar_InfernoMissileFile, Fini_GVar_InfernoMissileFile, &GVar_InfernoMissileFile, "InfernoMissileFile", 0, "Default Model for Inferno Missile", "Models/Weapons/Inferno/Inferno.M3D")
FINI_GVAR_STRING(Fini_GVar_InfernoExplosionFile, &GVar_InfernoExplosionFile)
INIT_GVAR_STRING(Init_GVar_InfernoExplosionFile, Fini_GVar_InfernoExplosionFile, &GVar_InfernoExplosionFile, "InfernoExplosionFile", 0, "Default Model for Inferno Explosion", "Models/Weapons/Inferno/Explosion.M3D")
INIT_GVAR_FLOAT(Init_GVar_InfernoNormSpeed, &GVar_InfernoNormSpeed, "InfernoNormSpeed", 0, "The Formal Speed that have the Inferno", 12000.0f)
INIT_GVAR_FLOAT(Init_GVar_InfernoBeesSpeed, &GVar_InfernoBeesSpeed, "InfernoBeesSpeed", 0, "The Internal Bees Speed that have the Inferno", 2000.0f)
INIT_GVAR_FLOAT(Init_GVar_InfernoUpTopSpeed, &GVar_InfernoUpTopSpeed, "InfernoUpTopSpeed", 0, "The Up Speed (Vehicle) that have the Inferno", 4000.0f)
INIT_GVAR_FLOAT(Init_GVar_InfernoTurnSpeed, &GVar_InfernoTurnSpeed, "InfernoTurnSpeed", 0, "The Turn Speed that have the Inferno", 20.0f)
INIT_GVAR_FLOAT(Init_GVar_InfernoTimeDelay, &GVar_InfernoTimeDelay, "InfernoTimeDelay", 0, "Time in seconds between the mis. shoots", 3.0f)
INIT_GVAR_FLOAT(Init_GVar_InfernoHitForce, &GVar_InfernoHitForce, "InfernoHitForce", 0, "The Hit Force Energy that have the Inferno cm*TN/sec", 75000.0f)
INIT_GVAR_FLOAT(Init_GVar_InfernoRangeMin, &GVar_InfernoRangeMin, "InfernoRangeMin", 0, "Range without bays that have the Inferno", 10000.0f)
INIT_GVAR_FLOAT(Init_GVar_InfernoRangeBay, &GVar_InfernoRangeBay, "InfernoRangeBay", 0, "Range add per bay that have the Inferno", 10000.0f)
INIT_GVAR_INT(Init_GVar_InfernoAmmoCost, &GVar_InfernoAmmoCost, "InfernoAmmoCost", 0, "Inferno Ammo Cost", 125)
FINI_GVAR_STRING(Fini_GVar_InfernoEngineSound, &GVar_InfernoEngineSound)
INIT_GVAR_STRING(Init_GVar_InfernoEngineSound, Fini_GVar_InfernoEngineSound, &GVar_InfernoEngineSound, "InfernoEngineSound", 0, "Inferno Engine sound name", "InfernoEngine")
INIT_GVAR_FLOAT(Init_GVar_InfernoEngineSoundVol, &GVar_InfernoEngineSoundVol, "InfernoEngineSoundVol", 0, "Inferno Engine sound volume", 0.75f)
INIT_GVAR_FLOAT(Init_GVar_InfernoEngineSoundAttIni, &GVar_InfernoEngineSoundAttIni, "InfernoEngineSoundAttIni", 0, "Inferno Engine sound attenuation ini", 5000.0f)
INIT_GVAR_FLOAT(Init_GVar_InfernoEngineSoundAttEnd, &GVar_InfernoEngineSoundAttEnd, "InfernoEngineSoundAttEnd", 0, "Inferno Engine sound attenuation end", 45000.0f)
INIT_GVAR_FLOAT(Init_GVar_InfernoEngineSoundDoppler, &GVar_InfernoEngineSoundDoppler, "InfernoEngineSoundDoppler", 0, "Inferno Engine sound doppler", 1.0f)
FINI_GVAR_STRING(Fini_GVar_InfernoHitSound, &GVar_InfernoHitSound)
INIT_GVAR_STRING(Init_GVar_InfernoHitSound, Fini_GVar_InfernoHitSound, &GVar_InfernoHitSound, "InfernoHitSound", 0, "Inferno Hit sound name", "InfernoBoom")
INIT_GVAR_FLOAT(Init_GVar_InfernoHitSoundVol, &GVar_InfernoHitSoundVol, "InfernoHitSoundVol", 0, "Inferno Hit sound volume", 1.0f)
INIT_GVAR_FLOAT(Init_GVar_InfernoHitSoundAttIniRadius, &GVar_InfernoHitSoundAttIniRadius, "InfernoHitSoundAttIniRadius", 0, "Inferno Hit sound attenuation ini by radius", 0.8f)
INIT_GVAR_FLOAT(Init_GVar_InfernoHitSoundAttEndRadius, &GVar_InfernoHitSoundAttEndRadius, "InfernoHitSoundAttEndRadius", 0, "Inferno Hit sound attenuation end by radius", 1.8f)
INIT_GVAR_FLOAT(Init_GVar_InfernoMinDamage, &GVar_InfernoMinDamage, "InfernoMinDamage", 0, "Minimal damage of Inferno", 5.0f)
INIT_GVAR_FLOAT(Init_GVar_InfernoMaxDamage, &GVar_InfernoMaxDamage, "InfernoMaxDamage", 0, "Maximun damage of Inferno", 225.0f)
INIT_GVAR_FLOAT(Init_GVar_InfernoUpgFireTime, &GVar_InfernoUpgFireTime, "InfernoUpgFireTime", 0, "Inferno Upgrade Fire Time", 20.0f)
INIT_GVAR_FLOAT(Init_GVar_InfernoUpgFireFactor, &GVar_InfernoUpgFireFactor, "InfernoUpgFireFactor", 0, "Inferno Upgrade Fire Factor", 4.0f)
INIT_GVAR_FLOAT(Init_GVar_InfernoInvUpgExtraDamage, &GVar_InfernoInvUpgExtraDamage, "InfernoInvUpgExtraDamage", 0, "Inferno Upgrade (inversed extended damage factor)", 0.9f)
INIT_GVAR_FLOAT(Init_GVar_FXInfernoExplosionMaxDist, &GVar_FXInfernoExplosionMaxDist, "FXInfernoExplosionMaxDist", 0, "Maximun Distance to show the Inferno Explosion Effect", 400000.0f)
INIT_NATIVE_FUNC(Init_NativeFunc_InfernoXplosionCall, &NativeFunc_InfernoXplosionCall, "InfernoXplosionCall", Inferno_SphereHit)
INIT_GVAR_INT(Init_GVar_CannonScorerColorRed, &GVar_CannonScorerColorRed, "CannonScorerColorRed", 0, "Cannon Color Red", 251)
INIT_GVAR_INT(Init_GVar_CannonScorerColorGreen, &GVar_CannonScorerColorGreen, "CannonScorerColorGreen", 0, "Cannon Color Green", 241)
INIT_GVAR_INT(Init_GVar_CannonScorerColorBlue, &GVar_CannonScorerColorBlue, "CannonScorerColorBlue", 0, "Cannon Color Blue", 174)
INIT_NATIVE_FUNC(Init_NativeFunc_InfernoPress, &NativeFunc_InfernoPress, "InfernoPress", Inferno_AimMode3Native)
INIT_NATIVE_FUNC(Init_NativeFunc_InfernoRelease, &NativeFunc_InfernoRelease, "InfernoRelease", Inferno_AimMode2Native)
INIT_NATIVE_FUNC(Init_NativeFunc_InfernoOnEnd, &NativeFunc_InfernoOnEnd, "InfernoOnEnd", Inferno_AimOffNative)
INIT_NATIVE_FUNC(Init_NativeFunc_InfernoOnStart, &NativeFunc_InfernoOnStart, "InfernoOnStart", Inferno_AimOnNative)
INIT_NATIVE_FUNC(Init_NativeFunc_InfernoHit, &NativeFunc_InfernoHit, "InfernoHit", Inferno_HitNative)
INIT_NATIVE_FUNC(Init_NativeFunc_InfernoServerHit, &NativeFunc_InfernoServerHit, "InfernoServerHit", Inferno_ServerHitNative)
INIT_NATIVE_FUNC(Init_NativeFunc_InfernoOnObjSlot, &NativeFunc_InfernoOnObjSlot, "InfernoOnObjSlot", Inferno_SlowNative)
INIT_NATIVE_FUNC(Init_NativeFunc_InfernoCreate, &NativeFunc_InfernoCreate, "InfernoCreate", Inferno_NetCreateNative)
INIT_NATIVE_FUNC(Init_NativeFunc_InfernoSetUp, &NativeFunc_InfernoSetUp, "InfernoSetUp", Inferno_NetSetUpNative)
INIT_NATIVE_FUNC(Init_NativeFunc_InfernoFire, &NativeFunc_InfernoFire, "InfernoFire", Inferno_FireNative)
INIT_NATIVE_FUNC(Init_NativeFunc_InfernoFX, &NativeFunc_InfernoFX, "InfernoFX", Inferno_HudNative)
